#include "delay.h"
#include "sys.h"
////////////////////////////////////////////////////////////////////////////////// 	 
//�����Ҫʹ��OS,����������ͷ�ļ�����.
#if SYSTEM_SUPPORT_OS
#include "FreeRTOS.h"					//FreeRTOSʹ��
#include "task.h"   
#endif

static u8  fac_us=0;							//us��ʱ������			   
static u16 fac_ms=0;							//ms��ʱ������,��ucos��,����ÿ�����ĵ�ms��
	
#if SYSTEM_SUPPORT_OS							//���SYSTEM_SUPPORT_OS������,˵��Ҫ֧��OS��(������UCOS).
//��delay_us/delay_ms��Ҫ֧��OS��ʱ����Ҫ������OS��صĺ궨��ͺ�����֧��
//������3���궨��:
//    delay_osrunning:���ڱ�ʾOS��ǰ�Ƿ���������,�Ծ����Ƿ����ʹ����غ���
//delay_ostickspersec:���ڱ�ʾOS�趨��ʱ�ӽ���,delay_init�����������������ʼ��systick
// delay_osintnesting:���ڱ�ʾOS�ж�Ƕ�׼���,��Ϊ�ж����治���Ե���,delay_msʹ�øò����������������
//Ȼ����3������:
//  delay_osschedlock:��������OS�������,��ֹ����
//delay_osschedunlock:���ڽ���OS�������,���¿�������
//    delay_ostimedly:����OS��ʱ,���������������.

extern void xPortSysTickHandler(void);
//systick �жϷ�����,ʹ�� OS ʱ�õ�
void SysTick_Handler(void)
{ 
 if(xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED)//ϵͳ�Ѿ�����
 {
 xPortSysTickHandler();
 }
}
#endif

			   
//��ʼ���ӳٺ���
//SYSTICK ��ʱ�ӹ̶�Ϊ AHB ʱ�ӣ������������� SYSTICK ʱ��Ƶ��Ϊ AHB/8
//����Ϊ�˼��� FreeRTOS�����Խ� SYSTICK ��ʱ��Ƶ�ʸ�Ϊ AHB ��Ƶ�ʣ�
//SYSCLK:ϵͳʱ��Ƶ��
void delay_init()
{
	u32 reload;
	SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK);//ѡ���ⲿʱ�� HCLK
	
	fac_us=SystemCoreClock/1000000; //�����Ƿ�ʹ�� OS,fac_us ����Ҫʹ��
	reload=SystemCoreClock/1000000; //ÿ���ӵļ������� ��λΪ M 
	reload*=1000000/configTICK_RATE_HZ; //���� configTICK_RATE_HZ �趨���
	//ʱ�� reload Ϊ 24 λ�Ĵ���,���ֵ:
	//16777216,�� 72M ��,Լ�� 0.233s ����
	fac_ms=1000/configTICK_RATE_HZ; //���� OS ������ʱ�����ٵ�λ 
	SysTick->CTRL|=SysTick_CTRL_TICKINT_Msk; //���� SYSTICK �ж�
	SysTick->LOAD=reload; //ÿ 1/configTICK_RATE_HZ ���ж�
	//һ��
	SysTick->CTRL|=SysTick_CTRL_ENABLE_Msk; //���� SYSTICK
}								    

#if SYSTEM_SUPPORT_OS  							//�����Ҫ֧��OS.
//��ʱ nus
//nus:Ҫ��ʱ�� us ��.
//nus:0~204522252(���ֵ�� 2^32/fac_us@fac_us=168) 
void delay_us(u32 nus)
{
	u32 ticks;
	u32 told,tnow,tcnt=0;
	u32 reload=SysTick->LOAD; //LOAD ��ֵ 
	ticks=nus*fac_us; //��Ҫ�Ľ�����
	told=SysTick->VAL; //�ս���ʱ�ļ�����ֵ
	while(1)
	{
		tnow=SysTick->VAL;
		if(tnow!=told)
		{
			//����ע��һ�� SYSTICK ��һ���ݼ��ļ������Ϳ�����. 
			if(tnow<told)tcnt+=told-tnow;
			else tcnt+=reload-tnow+told; 
			told=tnow;
			if(tcnt>=ticks)break; //ʱ�䳬��/����Ҫ�ӳٵ�ʱ��,���˳�.
		} 
	}; 
}
//��ʱ nms,�������������
//nms:Ҫ��ʱ�� ms ��
//nms:0~65535
void delay_ms(u32 nms)
{
	if(xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED)//ϵͳ�Ѿ�����
	{
		if(nms>=fac_ms) //��ʱ��ʱ����� OS ������ʱ������
		{ 
			vTaskDelay(nms/fac_ms); //FreeRTOS ��ʱ
		}
		nms%=fac_ms; //OS �Ѿ��޷��ṩ��ôС����ʱ��,
		//������ͨ��ʽ��ʱ 
	}
	delay_us((u32)(nms*1000)); //��ͨ��ʽ��ʱ
}
//��ʱ nms,���������������
//nms:Ҫ��ʱ�� ms ��
void delay_xms(u32 nms)
{
u32 i;
for(i=0;i<nms;i++) delay_us(1000);
}
#else //����OSʱ
//��ʱnus
//nusΪҪ��ʱ��us��.		    								   
void delay_us(u32 nus)
{		
	u32 temp;	    	 
	SysTick->LOAD=nus*fac_us; 					//ʱ�����	  		 
	SysTick->VAL=0x00;        					//��ռ�����
	SysTick->CTRL|=SysTick_CTRL_ENABLE_Msk ;	//��ʼ����	  
	do
	{
		temp=SysTick->CTRL;
	}while((temp&0x01)&&!(temp&(1<<16)));		//�ȴ�ʱ�䵽��   
	SysTick->CTRL&=~SysTick_CTRL_ENABLE_Msk;	//�رռ�����
	SysTick->VAL =0X00;      					 //��ռ�����	 
}
//��ʱnms
//ע��nms�ķ�Χ
//SysTick->LOADΪ24λ�Ĵ���,����,�����ʱΪ:
//nms<=0xffffff*8*1000/SYSCLK
//SYSCLK��λΪHz,nms��λΪms
//��72M������,nms<=1864 
void delay_ms(u16 nms)
{	 		  	  
	u32 temp;		   
	SysTick->LOAD=(u32)nms*fac_ms;				//ʱ�����(SysTick->LOADΪ24bit)
	SysTick->VAL =0x00;							//��ռ�����
	SysTick->CTRL|=SysTick_CTRL_ENABLE_Msk ;	//��ʼ����  
	do
	{
		temp=SysTick->CTRL;
	}while((temp&0x01)&&!(temp&(1<<16)));		//�ȴ�ʱ�䵽��   
	SysTick->CTRL&=~SysTick_CTRL_ENABLE_Msk;	//�رռ�����
	SysTick->VAL =0X00;       					//��ռ�����	  	    
} 
#endif 








































