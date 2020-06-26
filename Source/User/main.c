#define _main_c
#define THIS_VERSION ("GPRS V2.90(T) 20190703")
//#define DEBUG_MODE
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "101_Protocol.h"
#include "Flash.h"
#include "MG301.h"
#include "Para_Config.h"
#include "bsp_bkp.h"
#include "conf_GPIO.h"
#include "conf_NVIC.h"
#include "conf_RCC.h"
#include "conf_USART.h"
#include "conf_sys.h"
#include "conf_tim2.h"
#include "crc.h"
#include "ds3231.h"
#include "fun.h"
#include "gprs.h"
#include "i2c_ee.h"
#include "nrf905.h"
#include "stm32f10x.h"
#include "string.h"
#include "user_Configuration.h"

#define BVL GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_0) //��ص�ѹ����

/*��̬��������*/
//static uint16_t PowerOK_flag;
static uint16_t ReceiveDataFromGPRSflg;

volatile uint16_t SysTick_10msflg; //10ms�����־��������10ms����һ��
DEVICE_SET user_Set;
uint16_t GPRSLEDStat;
uint16_t info_wr_flash_flag;
uint16_t temp_wr_flash_flag;
uint16_t DebugDly; //���ڴ��ڵ�����ʱ
uint16_t DebugNRF905Dly;
uint16_t nrf905ReaddRegDly;
uint32_t nrf905InitDly;

uint16_t ReceiveLength;

uint8_t Info[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; //���������� 0/1/2 ���� 3����״̬ 4�¶�״̬ 5 Ƿѹ 6/7/8/9 ©��
uint16_t InfoCRC16; //ÿ�����ݶ�д֮ǰ��ҪУ�����ݵ���Ч�ԣ���ЧʱҪ���´�FLASH�ж�ȡ
uint8_t InfoTemp[8]; //�¶�
uint8_t DataFromGPRSBuffer[64];
uint8_t moduleMaskEn; // ģ�����MASK
uint32_t moduleMaskDly;
uint32_t PowerLowDly;
uint32_t PowerOKDly;

uint32_t InfoDisConnectDelay[3];
uint32_t TempDisConnectDelay[3];
uint8_t ReceiveData[100];

unsigned int nrf905DR;
extern uint16_t reqVersionflg;
extern uint16_t GPRSStat;
extern TimeStructure Tx_Time;

//��������1�����󷵻�0
//ÿ�ζ�д��Ҫ����У��CRC���д�ʱ�����´�FLASH�ж�ȡ����
uint16_t CheckInfoCRCIsOK(void) {
	if (CRC16(Info, 16) == InfoCRC16) {
		return 1;
	}
	else {
		return 0;
	}
}

//���ݸı�����CRC
void RefreshInfoCRC(void) {
	InfoCRC16 = CRC16(Info, 16);
}

void InitAllPara(void) {
	//������Է�������ַ
#ifdef DEBUG_MODE

	user_Set.ip_len = 15;
	memcpy(user_Set.ip_info, "\"218.29.54.111\"", 15);
	user_Set.port_len = 5;
	memcpy(user_Set.port_info, "20001", 5);

	//user_Set.ip_len = 13;
	//memcpy(user_Set.ip_info,"180.97.237.80",13);
	//user_Set.port_len = 4;
	//memcpy(user_Set.port_info,"5005",4);
#else

	//user_Set.ip_len = 13;
	//memcpy(user_Set.ip_info,"120.25.73.254",13);
	//user_Set.port_len = 4;
	//memcpy(user_Set.port_info,"4002",4);
	//ʵ�ʷ�������ַ
	user_Set.ip_len = 15;
	memcpy(user_Set.ip_info, "\"218.29.54.111\"", 15);
	user_Set.port_len = 5;
	memcpy(user_Set.port_info, "20001", 5);
#endif
	user_Set.addr_len = 1;
	memcpy(user_Set.addr_info, "1", 1);
	user_Set.ModuleID[0] = 0x2f;
	user_Set.ModuleID[1] = 0x18;
	user_Set.ModuleID[2] = 0xff;
	user_Set.ModuleID[3] = 0xf1;
	//user_Set.ModuleID[0] = 0xFF;
	//user_Set.ModuleID[1] = 0xFF;
	//user_Set.ModuleID[2] = 0xFF;
	//user_Set.ModuleID[3] = 0xFF;
	ReceiveDataFromGPRSflg = 0;
	memset(DataFromGPRSBuffer, 0, 64);

	user_Set.apn_len = 5;
	memcpy(user_Set.apn_info, "ctnet", 5);
	user_Set.user_len = 2;
	memcpy(user_Set.user_info, "cm", 2);
	user_Set.password_len = 4;
	memcpy(user_Set.password_info, "gprs", 4);
	user_Set.heart_len = 7;
	{
		unsigned char heart[9] = {0x20, 0x18, 0x11, 0x01, 0x00, 0x67, 0x00};
		memcpy(user_Set.heart_info, (char*)heart, 9);
	}
	//��ʵ120S��һ��
#ifdef DEBUG_MODE
	//������10S�ӷ�һ��
	user_Set.heart_time_len = 2;
	memcpy(user_Set.heart_time_info, "40", 2);
	user_Set.FirstUsedFlag = 0x3378;
#else
	user_Set.heart_time_len = 2;
	memcpy(user_Set.heart_time_info, "40", 2);
	user_Set.FirstUsedFlag = 0x3378;
#endif
}

static uint16_t run_loop_cnt;

int main(void) {
	run_loop_cnt = 0;
	info_wr_flash_flag = 0;
	temp_wr_flash_flag = 0;
	DebugNRF905Dly = 0 * 50 * 60;
	//Ƭ���豸��ʼ��
	RCC_Configuration();
	SysTick_Init(); //10ms�ж�һ��
	NVIC_Configuration();
	GPIO_Configuration();
	USART1_Configuration(); //RS232����ͨ��
	USART3_Configuration(); //GPRSͨ��
	EXTI_Configuration(); //NRF595�жϽſ���
	SYSCLKConfig_STOP(); //��������
	//Ƭ���豸��ʼ��

	//work led	on
	GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_RESET); //����
	Delay(10);
	PowerUp_NRF905();
	Delay(50);

	FLASH_ReadUserSet();

	if (user_Set.FirstUsedFlag != 0x3378) {
		InitAllPara(); //��ʼ������
		FLASH_WriteUserSet();
	}

	{
		uint8_t Temp[16];
		memset(Temp, 0x00, 16);
		memcpy(Temp, user_Set.addr_info, user_Set.addr_len);
		LINK_ADDRESS = Str2Int((char*)Temp);
		memset(Temp, 0x00, 16);
	}

	FLASH_RD_Module_Status();

#ifndef DEBUG_MODE
	IWDG_Init(IWDG_Prescaler_16, 0xFFF); //1.6s
	IWDG_Feed(); //clr WDG
#endif
	GPRS_init(); //������ʼ��
	//���¶�ֵ
	PWR_BackupAccessCmd(ENABLE); //ʹ�ܺ󱸼Ĵ�������
	if ((BKP_ReadBackupRegister(BKP_DR1) != 0XA55A) || bkp_ReadTempData() != 0) {
		BKP_WriteBackupRegister(BKP_DR1, 0XA55A);
		InfoTemp[0] = 30;
		InfoTemp[1] = 30;
		InfoTemp[2] = 30;
		InfoTemp[3] = 30;
		InfoTemp[4] = 30;
		InfoTemp[5] = 30;
		InfoTemp[6] = 30;
		InfoTemp[7] = 30;
		bkp_WriteTempData(); //��ʼ��һ������
	}

	EXTI_ClearITPendingBit(EXTI_Line11);
	EnableExtINT();

	DebugDly = 0; //0����ʱ��
	PowerLowDly = 0;
	PowerOKDly = 0;
	nrf905ReaddRegDly = 50 * 1 * 1;
	nrf905InitDly = 50 * 60 * 60 * 47; //48��Сʱǿ�Ƴ�ʼ��һ��
	InfoDisConnectDelay[0] = 50 * 60 * 60 * 12;
	InfoDisConnectDelay[1] = 50 * 60 * 60 * 12;
	InfoDisConnectDelay[2] = 50 * 60 * 60 * 12;

	TempDisConnectDelay[0] = 50 * 60 * 60 * 12;
	TempDisConnectDelay[1] = 50 * 60 * 60 * 12;
	TempDisConnectDelay[2] = 50 * 60 * 60 * 12;

	moduleMaskEn = 0;
	moduleMaskDly = 0;
	while (1) {
#ifdef DEBUG_MODE
		GPIO_SetBits(GPIOB, GPIO_Pin_15); //����GPRSģ�鿪�����ŵ�ƽ
		DelayMs(500);
		GPIO_ResetBits(GPIOB, GPIO_Pin_15);
		USART3_InitRXbuf();
		USART3_SendDataToGPRS("AT+CSQ\r", strlen("AT+CSQ\r"));
		DelayMs(20);
		ReceiveLength = Supervise_USART3(ReceiveData);
		USART3_SendDataToGPRS("AT+CGDCONT=1,\"IP\",\"CTNET\"", strlen("AT+CGDCONT=1,\"IP\",\"CTNET\""));
		DelayMs(20);
		ReceiveLength = Supervise_USART3(ReceiveData);
		USART3_SendDataToGPRS("AT+CIPMODE=1", strlen("AT+CIPMODE=1"));
		DelayMs(20);
		ReceiveLength = Supervise_USART3(ReceiveData);
		USART3_SendDataToGPRS("AT+NETOPEN", strlen("AT+NETOPEN"));
		DelayMs(20);
		ReceiveLength = Supervise_USART3(ReceiveData);
		USART3_SendDataToGPRS("AT+CIPOPEN=0,\"TCP\",\"218.29.54.111\",20001", strlen("AT+CIPOPEN=0,\"TCP\",\"218.29.54.111\",20001"));
		DelayMs(20);
		ReceiveLength = Supervise_USART3(ReceiveData);
		USART3_SendDataToGPRS("Hello, TCP", strlen("Hello, TCP"));
		USART3_SendDataToGPRS("+++", strlen("+++"));
		DelayMs(20);
		ReceiveLength = Supervise_USART3(ReceiveData);
		USART3_SendDataToGPRS("ATO", strlen("ATO"));
		DelayMs(20);
		USART3_SendDataToGPRS("Hello, IP", strlen("Hello, IP"));
		USART3_SendDataToGPRS("+++", strlen("+++"));
		DelayMs(20);
		USART3_SendDataToGPRS("AT+CIPCLOSE=0", strlen("AT+CIPCLOSE=0"));
		DelayMs(20);
		USART3_SendDataToGPRS("AT+NETCLOSE", strlen("AT+NETCLOSE"));
		DelayMs(20);
#else
		if (SysTick_10msflg) {
			SysTick_10msflg = 0;
			if (DebugDly > 0)
				DebugDly--;
			if (moduleMaskDly > 0)
				moduleMaskDly--;
			else
				moduleMaskEn = 0; //����24Сʱ���Զ��˳�

			if (reqVersionflg) {
				reqVersionflg = 0;
				USART1_SendDataToRS232(THIS_VERSION, strlen(THIS_VERSION));
			}
			//--------------------------------
			if (DebugNRF905Dly > 0)
				DebugNRF905Dly--;
			if (nrf905InitDly > 0)
				nrf905InitDly--;

			if (InfoDisConnectDelay[0])
				InfoDisConnectDelay[0]--;
			if (InfoDisConnectDelay[1])
				InfoDisConnectDelay[1]--;
			if (InfoDisConnectDelay[2])
				InfoDisConnectDelay[2]--;

			if (TempDisConnectDelay[0])
				TempDisConnectDelay[0]--;
			if (TempDisConnectDelay[1])
				TempDisConnectDelay[1]--;
			if (TempDisConnectDelay[2])
				TempDisConnectDelay[2]--;

			if ((InfoDisConnectDelay[0] == 0) || (InfoDisConnectDelay[1] == 0) || (InfoDisConnectDelay[2] == 0)) {
				if (Info[3] == 0) { //״̬�ı䱣��һ��
					DisableExtINT();
					if (CheckInfoCRCIsOK() == 0) { //ÿ�θı�Info����֮ǰ��Ҫ����
						FLASH_RD_Module_Status();
					}
					Info[3] = 0x01;
					RefreshInfoCRC();
					info_wr_flash_flag = 1;
					EnableExtINT();
				}
			}
			else {
				if (Info[3] == 1) { //״̬�ı䱣��һ��
					DisableExtINT();
					if (CheckInfoCRCIsOK() == 0) { //ÿ�θı�Info����֮ǰ��Ҫ����
						FLASH_RD_Module_Status();
					}
					Info[3] = 0x00;
					RefreshInfoCRC();
					info_wr_flash_flag = 1;
					EnableExtINT();
				}
			}
			//�¶Ȳ����߲���
			if ((TempDisConnectDelay[0] == 0) || (TempDisConnectDelay[1] == 0) || (TempDisConnectDelay[2] == 0)) {
				if (Info[4] == 0) { //״̬�ı䱣��һ��
					DisableExtINT();
					if (CheckInfoCRCIsOK() == 0) { //ÿ�θı�Info����֮ǰ��Ҫ����
						FLASH_RD_Module_Status();
					}
					Info[4] = 0x01;
					RefreshInfoCRC();
					info_wr_flash_flag = 1;
					EnableExtINT();
				}
			}
			else {
				if (Info[4] == 1) { //״̬�ı䱣��һ��
					DisableExtINT();
					if (CheckInfoCRCIsOK() == 0) { //ÿ�θı�Info����֮ǰ��Ҫ����
						FLASH_RD_Module_Status();
					}
					Info[4] = 0x00;
					RefreshInfoCRC();
					info_wr_flash_flag = 1;
					EnableExtINT();
				}
			}

			if (GPIO_ReadInputDataBit(NRF905_DR, NRF905_DR_PIN)) { //�ж���δ��������������ʱ ��ʼ���жϷ�����򲢳�ʼ��NRF905
				Delay(10);
				if (GPIO_ReadInputDataBit(NRF905_DR, NRF905_DR_PIN)) {
					DisableExtINT();
					PowerUp_NRF905();
					NVIC_Configuration();
					EnableExtINT();
				}
			}

			/*���NRF905����Ч��*/
			if (nrf905ReaddRegDly > 0) {
				nrf905ReaddRegDly--;
			}
			else {
				uint16_t nrf905errorflg;

				nrf905errorflg = 0;
				nrf905ReaddRegDly = 50 * 20; //20S��һ�μĴ���ֵ
				DisableExtINT();
				if (checknrf905_conf() == 0) { //����
					nrf905errorflg = 1;
				}
				if (checknrf905_addr() == 0) { //����
					nrf905errorflg = 1;
				}
				if (nrf905errorflg == 1 || (nrf905InitDly == 0)) {
					nrf905errorflg = 0;
					nrf905InitDly = 50 * 60 * 60 * 47; //48��Сʱǿ�Ƴ�ʼ��һ��
					PowerUp_NRF905();
				}
				EnableExtINT();
			}
			//LED_Toggle();
#ifndef DEBUG_MODE
			IWDG_Feed(); //clr WDG
#endif
			//һ����⵽TCPͨ���Ͽ���������������
			ReceiveDataFromGPRSflg = SuperviseTCP(DataFromGPRSBuffer);
			if (ReceiveDataFromGPRSflg) {
				DataProcess();
			}

			//���ô��ڴ�������
			rs232_set_process();
			USART1_supervise(); //��ʱ��û�н��յ�����ʱ��һ������
			//��ص�ѹ��⣬����ʱBVLΪ�͵�ƽ
			if (!BVL) //��ѹ��
			{
				PowerOKDly = 0;
				if (CheckInfoCRCIsOK() == 0) {
					FLASH_RD_Module_Status();
				}

				if (Info[5] == 0) //��ǰ״̬Ϊ�ߵ�ѹ״̬
				{
					if (PowerLowDly < 50 * 60 * 10) { //��ʱ10���ӱ���ѹ��
						PowerLowDly++;
					}
					else {
						GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_RESET);
						//read time
						ReadDATATime();
						ChangeUpdate(0x06, 0x01, &Tx_Time);
						if (CheckInfoCRCIsOK() == 0) { //ÿ�θı�Info����֮ǰ��Ҫ����
							FLASH_RD_Module_Status();
						}
						Info[5] = 0x01;
						RefreshInfoCRC();
						info_wr_flash_flag = 1;
						//USART1_SendDataToRS232("Module Power is Low\r\n",21);
						//work led off
						GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_SET);
					}
				}
			}
			else { //Power is ok

				PowerLowDly = 0;
				if (CheckInfoCRCIsOK() == 0) {
					FLASH_RD_Module_Status();
				}
				if (Info[5] != 0) {
					if (PowerOKDly < 50 * 60 * 30) { //30���ӱ�����
						PowerOKDly++;
					}
					else {
						GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_RESET);
						//read time
						ReadDATATime();
						ChangeUpdate(0x06, 0x00, &Tx_Time);
						if (CheckInfoCRCIsOK() == 0) {
							FLASH_RD_Module_Status();
						}
						Info[5] = 0x00;
						RefreshInfoCRC();
						info_wr_flash_flag = 1;
						//USART1_SendDataToRS232("Module Power is OK\r\n",20);
						//work led off
						GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_SET);
					}
				}
			}

			//
			if (info_wr_flash_flag == 1) {
				FLASH_WR_Module_Status();
				info_wr_flash_flag = 0;
			}
			if (temp_wr_flash_flag == 1) {
				bkp_WriteTempData();
				temp_wr_flash_flag = 0;
			}

			if (run_loop_cnt > 0)
				run_loop_cnt--;
			switch (GPRSLEDStat) {
			case GPRS_LED_IDLE:
				if (GPRSStat < 0x700) { //����״̬
					if (((run_loop_cnt) == 14)) //��������һ��
					{
						GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_RESET); //work led on
					}
					else if (run_loop_cnt == 12) {
						GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_SET);
					}
					else {
						if (run_loop_cnt == 0)
							run_loop_cnt = 15;
					}
				}
				else {
					GPRSLEDStat = GPRSGetStatBuf();
					if (GPRSLEDStat == GPRS_LED_IDLE) { //�����õĵȴ�״̬4s����4mS
						if (GPRSStat == GPRS_RUN_Txdata) { //�ȴ�ָ��ؼ���������
							if (run_loop_cnt == 49) {
								GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_RESET); //work led on
							}
							else if (run_loop_cnt == 47) {
								GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_SET);
							}
							else if (run_loop_cnt == 0) { //2S��һ��
								run_loop_cnt = 50;
							}
						}
						else if (GPRSStat == GPRS_RUN_Txdata_ACK) { //�ȴ����ݷ�����ȷ
							if (run_loop_cnt == 49) {
								GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_RESET); //work led on
							}
							else if (run_loop_cnt == 47) {
								GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_SET);
							}
							else if (run_loop_cnt == 0) {
								run_loop_cnt = 50;
							}
						}
						else { //�������еĿ���״̬4S��һ��

							if (run_loop_cnt == 199) {
								GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_RESET); //work led on
							}
							else if (run_loop_cnt == 194) {
								GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_SET);
							}
							else if (run_loop_cnt == 0) { //4S��һ��
								GPRSLEDStat = GPRSGetStatBuf();
								run_loop_cnt = 200;
							}
						}
					}
					else { //���뷢��״̬
						run_loop_cnt = 50;
					}
				}
				break;
			case GPRS_LED_START: //һ��
				if (run_loop_cnt == 49) {
					GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_RESET); //work led on
				}
				else if (run_loop_cnt == 35) {
					GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_SET);
				}
				else if (run_loop_cnt == 0) { //4S��һ��
					GPRSLEDStat = GPRSGetStatBuf();
					run_loop_cnt = 50;
				}
				break;
				//case GPRS_LED_CMD_OK:	//һ��һ��
			case GPRS_LED_DATA_OK: //
				if (run_loop_cnt == 49) {
					GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_RESET); //work led on
				}
				else if (run_loop_cnt == 35) {
					GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_SET);
				}
				else if (run_loop_cnt == 30) {
					GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_RESET); //work led on
				}
				else if (run_loop_cnt == 28) {
					GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_SET);
				}
				else if (run_loop_cnt == 0) {
					GPRSLEDStat = GPRSGetStatBuf();
					run_loop_cnt = 50;
				}
				break;
			case GPRS_LED_CMD_ERROR: //һ������
			case GPRS_LED_DATA_ERROR:
				if (run_loop_cnt == 49) {
					GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_RESET); //work led on
				}
				else if (run_loop_cnt == 35) {
					GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_SET);
				}
				else if (run_loop_cnt == 30) { //1��
					GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_RESET); //work led on
				}
				else if (run_loop_cnt == 28) {
					GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_SET);
				}
				else if (run_loop_cnt == 20) {
					GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_RESET); //work led on
				}
				else if (run_loop_cnt == 18) {
					GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_SET);
				}
				else if (run_loop_cnt == 0) {
					GPRSLEDStat = GPRSGetStatBuf();
					run_loop_cnt = 50;
				}
				break;
			case GPRS_LED_CMD_TO: //һ������
			case GPRS_LED_DATA_TO:
				if (run_loop_cnt == 49) {
					GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_RESET); //work led on
				}
				else if (run_loop_cnt == 38) {
					GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_SET);
				}
				else if (run_loop_cnt == 30) { //1��
					GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_RESET); //work led on
				}
				else if (run_loop_cnt == 28) {
					GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_SET);
				}
				else if (run_loop_cnt == 20) {
					GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_RESET); //work led on
				}
				else if (run_loop_cnt == 18) {
					GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_SET);
				}
				else if (run_loop_cnt == 10) {
					GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_RESET); //work led on
				}
				else if (run_loop_cnt == 8) {
					GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_SET);
				}
				else if (run_loop_cnt == 0) {
					GPRSLEDStat = GPRSGetStatBuf();
					run_loop_cnt = 100;
				}
				break;
			case GPRS_LED_DATA: //Ƶ��
				if ((run_loop_cnt & 0x2) != 0) {
					GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_RESET); //work led on
				}
				else {
					GPIO_WriteBit(ARM_RUN, ARM_RUN_PIN, Bit_SET);
				}
				if (run_loop_cnt == 0) {
					GPRSLEDStat = GPRSGetStatBuf();
					run_loop_cnt = 100;
				}
				break;
			default:
				GPRSLEDStat = GPRS_LED_IDLE;
				break;
			}

		} //if()
#endif
#ifndef DEBUG_MODE
		__WFI(); //����˯��ģʽ
#endif
	} //while
} //main

#ifdef USE_FULL_ASSERT

/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t* file, uint32_t line) {
	/* User can add his own implementation to report the file name and line number,
ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

	/* Infinite loop */
	while (1) {
	}
}

#endif

/**
 * @}
 */

/**
 * @}
 */

/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/