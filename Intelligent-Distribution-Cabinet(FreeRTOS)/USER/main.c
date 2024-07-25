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
����˵����
	BC20   ------> 	STM32
	PWRKEY			PA12	
	RESET			PA1
*/
extern int err;
extern int val;

#define 		START_TASK_PRIO 		1 		//�������ȼ�
#define 		START_STK_SIZE 			128 	//�����ջ��С
TaskHandle_t 	StartTask_Handler; 				//������
void start_task(void *pvParameters); 			//������

#define 		SEND_TASK_PRIO 			2 		//�������ȼ�
#define 		SEND_STK_SIZE 			1024 	//�����ջ��С
TaskHandle_t 	SENDTask_Handler; 				//������
void send_task(void *pvParameters); 			//������

int main(void)
{	
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);//����ϵͳ�ж����ȼ����� 4
	delay_init(); //��ʱ������ʼ��
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
	
	//������ʼ����
	xTaskCreate(	
		(TaskFunction_t)start_task,						//������
		(const char *  )"start_task",					//��������
		(uint16_t      )START_STK_SIZE,				//�����ջ��С
		(void *        )NULL,									//���ݸ��������Ĳ���
		(UBaseType_t   ) START_TASK_PRIO,			//�������ȼ�
		(TaskHandle_t *) &StartTask_Handler
	);	//������ 
	vTaskStartScheduler();															//����������� 	 
}

void start_task(void *pvParameters)
{
	taskENTER_CRITICAL(); //�����ٽ���
	
	//����SEND����
	xTaskCreate(
		(TaskFunction_t )send_task, 
		(const char* )"send_task", 
		(uint16_t )SEND_STK_SIZE, 
		(void* )NULL,
		(UBaseType_t )SEND_TASK_PRIO,
		(TaskHandle_t* )&SENDTask_Handler
	); 
							
	vTaskDelete(StartTask_Handler); //ɾ����ʼ����
	taskEXIT_CRITICAL(); //�˳��ٽ���
}

//LED0 ������
void send_task(void *pvParameters)
{
	char senddata[BUFLEN];
	char tempstr[BUFLEN];
	char *gpsStr; 							//GPSָ��λ��
	char sendMutiData[BUFLEN]; 	//�������ݵ�����
	char gpsDatalat[64];
	char gpsDatalon[64];
	while(1)
	{
		LED2 = !LED2;
		rn8302_ReadData();
		memset(tempstr,0,BUFLEN);
		memset(senddata,0,BUFLEN);
		gpsStr = NULL;
		memset(sendMutiData,0,BUFLEN);							//���Ͷഫ�������ݵ�������
		memset(gpsDatalat,0,64);										//ά��
		memset(gpsDatalon,0,64);										//����
		strcat(sendMutiData,"latitude=");						//����ά��
		gpsStr = Get_GPS_RMC(1);										//��ȡά�� ������ƫ֮���
								
		if(gpsStr)																	//�����ȡ����
		{
			strcat(sendMutiData,gpsStr);							//����ά��
			strcat(gpsDatalat,gpsStr);								//����ά��
		}
		else																				//��û��λ��
		{
			strcat(sendMutiData,"39.897445");					//����ά��
			strcat(gpsDatalat,"39.897445");						//����ά��
		}
								
		strcat(sendMutiData,"&longitude="); 				//���ݾ���
		gpsStr = Get_GPS_RMC(2);										//��ȡ���� ������ƫ֮���
		if(gpsStr)																	//�����ȡ����
		{
			strcat(sendMutiData,gpsStr); 							//���ݾ���
			strcat(gpsDatalon,gpsStr); 								//���ݾ���
		}
		else																				//��û��λ��
		{
			strcat(sendMutiData,"116.331398");				//���ݾ���
			strcat(gpsDatalon,"116.331398");					//���ݾ���
		}
		CSTX_4G_ONENETIOTSenddataPowerData(Power_Data,gpsDatalat,gpsDatalon);
		vTaskDelay(500);
	}
} 

