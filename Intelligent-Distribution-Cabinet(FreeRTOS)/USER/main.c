#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "bc20.h"
#include "gpio.h"
#include "rn8302.h"
#include "oled.h"
#include "bmp.h"
#include "semphr.h"

/*
接线说明：
	BC20   ------> 	STM32
	PWRKEY			PA12	
	RESET			PA1
*/
extern int err;
extern int val;

#define 		START_TASK_PRIO 		1 		//任务优先级
#define 		START_STK_SIZE 			128 	//任务堆栈大小
TaskHandle_t 	StartTask_Handler; 				//任务句柄
void start_task(void *pvParameters); 			//任务函数

#define 		SEND_TASK_PRIO 			2 		//任务优先级
#define 		SEND_STK_SIZE 			1024 	//任务堆栈大小
TaskHandle_t 	SENDTask_Handler; 				//任务句柄
void send_task(void *pvParameters); 			//任务函数

int main(void)
{	
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);//设置系统中断优先级分组 4
	delay_init(); //延时函数初始化
	OLED_Init();
	OLED_ColorTurn(0);
	OLED_DisplayTurn(0);
	BC20_GpioInit();
	GPIO_INIT();
	uart1_init(115200);
	uart2_init(115200);
	uart3_init(115200);
	NBIOT_RESET();
	OLED_Clear();
	OLED_ShowString(0,0,"STM32+BC20+WIFI",16,1);
	OLED_ShowString(0,16,"NBIOT GPS BOARD",16,1);
	OLED_Refresh();
	Uart1_SendStr("init stm32L COM1 \r\n"); 
	rn8302_SpiConfig();  
	rn8302_EmuInit();
	while(BC20_Init());
 	BC20_PDPACT();
	BC20_EmqxInit();
	BC20_INITGNSS();
	LED1 = 1;
	
	//创建开始任务
	xTaskCreate(	
		(TaskFunction_t)start_task,						//任务函数
		(const char *  )"start_task",					//任务名称
		(uint16_t      )START_STK_SIZE,				//任务堆栈大小
		(void *        )NULL,									//传递给任务函数的参数
		(UBaseType_t   ) START_TASK_PRIO,			//任务优先级
		(TaskHandle_t *) &StartTask_Handler
	);	//任务句柄 
	vTaskStartScheduler();															//开启任务调度 	 
}

void start_task(void *pvParameters)
{
	taskENTER_CRITICAL(); //进入临界区
	
	//创建SEND任务
	xTaskCreate(
		(TaskFunction_t )send_task, 
		(const char* )"send_task", 
		(uint16_t )SEND_STK_SIZE, 
		(void* )NULL,
		(UBaseType_t )SEND_TASK_PRIO,
		(TaskHandle_t* )&SENDTask_Handler
	); 
							
	vTaskDelete(StartTask_Handler); //删除开始任务
	taskEXIT_CRITICAL(); //退出临界区
}

//LED0 任务函数
void send_task(void *pvParameters)
{
	char senddata[BUFLEN];
	char tempstr[BUFLEN];
	char *gpsStr; 							//GPS指向位置
	char sendMutiData[BUFLEN]; 	//发送数据到服务
	char gpsDatalat[64];
	char gpsDatalon[64];
	while(1)
	{
		LED2 = !LED2;
		rn8302_ReadData();
		memset(tempstr,0,BUFLEN);
		memset(senddata,0,BUFLEN);
		gpsStr = NULL;
		memset(sendMutiData,0,BUFLEN);							//发送多传感器数据到服务器
		memset(gpsDatalat,0,64);										//维度
		memset(gpsDatalon,0,64);										//经度
		strcat(sendMutiData,"latitude=");						//传递维度
		gpsStr = Get_GPS_RMC(1);										//获取维度 经过纠偏之后的
								
		if(gpsStr)																	//如果读取到了
		{
			strcat(sendMutiData,gpsStr);							//传递维度
			strcat(gpsDatalat,gpsStr);								//传递维度
		}
		else																				//还没定位好
		{
			strcat(sendMutiData,"39.897445");					//传递维度
			strcat(gpsDatalat,"39.897445");						//传递维度
		}
								
		strcat(sendMutiData,"&longitude="); 				//传递经度
		gpsStr = Get_GPS_RMC(2);										//获取经度 经过纠偏之后的
		if(gpsStr)																	//如果读取到了
		{
			strcat(sendMutiData,gpsStr); 							//传递经度
			strcat(gpsDatalon,gpsStr); 								//传递经度
		}
		else																				//还没定位好
		{
			strcat(sendMutiData,"116.331398");				//传递经度
			strcat(gpsDatalon,"116.331398");					//传递经度
		}
		CSTX_4G_ONENETIOTSenddataPowerData(Power_Data,gpsDatalat,gpsDatalon);
		vTaskDelay(500);
	}
} 

