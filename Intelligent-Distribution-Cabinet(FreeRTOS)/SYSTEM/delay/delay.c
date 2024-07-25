#include "delay.h"
#include "sys.h"
////////////////////////////////////////////////////////////////////////////////// 	 
//如果需要使用OS,则包括下面的头文件即可.
#if SYSTEM_SUPPORT_OS
#include "FreeRTOS.h"					//FreeRTOS使用
#include "task.h"   
#endif

static u8  fac_us=0;							//us延时倍乘数			   
static u16 fac_ms=0;							//ms延时倍乘数,在ucos下,代表每个节拍的ms数
	
#if SYSTEM_SUPPORT_OS							//如果SYSTEM_SUPPORT_OS定义了,说明要支持OS了(不限于UCOS).
//当delay_us/delay_ms需要支持OS的时候需要三个与OS相关的宏定义和函数来支持
//首先是3个宏定义:
//    delay_osrunning:用于表示OS当前是否正在运行,以决定是否可以使用相关函数
//delay_ostickspersec:用于表示OS设定的时钟节拍,delay_init将根据这个参数来初始哈systick
// delay_osintnesting:用于表示OS中断嵌套级别,因为中断里面不可以调度,delay_ms使用该参数来决定如何运行
//然后是3个函数:
//  delay_osschedlock:用于锁定OS任务调度,禁止调度
//delay_osschedunlock:用于解锁OS任务调度,重新开启调度
//    delay_ostimedly:用于OS延时,可以引起任务调度.

extern void xPortSysTickHandler(void);
//systick 中断服务函数,使用 OS 时用到
void SysTick_Handler(void)
{ 
 if(xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED)//系统已经运行
 {
 xPortSysTickHandler();
 }
}
#endif

			   
//初始化延迟函数
//SYSTICK 的时钟固定为 AHB 时钟，基础例程里面 SYSTICK 时钟频率为 AHB/8
//这里为了兼容 FreeRTOS，所以将 SYSTICK 的时钟频率改为 AHB 的频率！
//SYSCLK:系统时钟频率
void delay_init()
{
	u32 reload;
	SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK);//选择外部时钟 HCLK
	
	fac_us=SystemCoreClock/1000000; //不论是否使用 OS,fac_us 都需要使用
	reload=SystemCoreClock/1000000; //每秒钟的计数次数 单位为 M 
	reload*=1000000/configTICK_RATE_HZ; //根据 configTICK_RATE_HZ 设定溢出
	//时间 reload 为 24 位寄存器,最大值:
	//16777216,在 72M 下,约合 0.233s 左右
	fac_ms=1000/configTICK_RATE_HZ; //代表 OS 可以延时的最少单位 
	SysTick->CTRL|=SysTick_CTRL_TICKINT_Msk; //开启 SYSTICK 中断
	SysTick->LOAD=reload; //每 1/configTICK_RATE_HZ 秒中断
	//一次
	SysTick->CTRL|=SysTick_CTRL_ENABLE_Msk; //开启 SYSTICK
}								    

#if SYSTEM_SUPPORT_OS  							//如果需要支持OS.
//延时 nus
//nus:要延时的 us 数.
//nus:0~204522252(最大值即 2^32/fac_us@fac_us=168) 
void delay_us(u32 nus)
{
	u32 ticks;
	u32 told,tnow,tcnt=0;
	u32 reload=SysTick->LOAD; //LOAD 的值 
	ticks=nus*fac_us; //需要的节拍数
	told=SysTick->VAL; //刚进入时的计数器值
	while(1)
	{
		tnow=SysTick->VAL;
		if(tnow!=told)
		{
			//这里注意一下 SYSTICK 是一个递减的计数器就可以了. 
			if(tnow<told)tcnt+=told-tnow;
			else tcnt+=reload-tnow+told; 
			told=tnow;
			if(tcnt>=ticks)break; //时间超过/等于要延迟的时间,则退出.
		} 
	}; 
}
//延时 nms,会引起任务调度
//nms:要延时的 ms 数
//nms:0~65535
void delay_ms(u32 nms)
{
	if(xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED)//系统已经运行
	{
		if(nms>=fac_ms) //延时的时间大于 OS 的最少时间周期
		{ 
			vTaskDelay(nms/fac_ms); //FreeRTOS 延时
		}
		nms%=fac_ms; //OS 已经无法提供这么小的延时了,
		//采用普通方式延时 
	}
	delay_us((u32)(nms*1000)); //普通方式延时
}
//延时 nms,不会引起任务调度
//nms:要延时的 ms 数
void delay_xms(u32 nms)
{
u32 i;
for(i=0;i<nms;i++) delay_us(1000);
}
#else //不用OS时
//延时nus
//nus为要延时的us数.		    								   
void delay_us(u32 nus)
{		
	u32 temp;	    	 
	SysTick->LOAD=nus*fac_us; 					//时间加载	  		 
	SysTick->VAL=0x00;        					//清空计数器
	SysTick->CTRL|=SysTick_CTRL_ENABLE_Msk ;	//开始倒数	  
	do
	{
		temp=SysTick->CTRL;
	}while((temp&0x01)&&!(temp&(1<<16)));		//等待时间到达   
	SysTick->CTRL&=~SysTick_CTRL_ENABLE_Msk;	//关闭计数器
	SysTick->VAL =0X00;      					 //清空计数器	 
}
//延时nms
//注意nms的范围
//SysTick->LOAD为24位寄存器,所以,最大延时为:
//nms<=0xffffff*8*1000/SYSCLK
//SYSCLK单位为Hz,nms单位为ms
//对72M条件下,nms<=1864 
void delay_ms(u16 nms)
{	 		  	  
	u32 temp;		   
	SysTick->LOAD=(u32)nms*fac_ms;				//时间加载(SysTick->LOAD为24bit)
	SysTick->VAL =0x00;							//清空计数器
	SysTick->CTRL|=SysTick_CTRL_ENABLE_Msk ;	//开始倒数  
	do
	{
		temp=SysTick->CTRL;
	}while((temp&0x01)&&!(temp&(1<<16)));		//等待时间到达   
	SysTick->CTRL&=~SysTick_CTRL_ENABLE_Msk;	//关闭计数器
	SysTick->VAL =0X00;       					//清空计数器	  	    
} 
#endif 








































