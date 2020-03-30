#include "usart.h"
#include "type.h"
#include "modbus_asc.h"
//#include "FlashIf.h"
#include "stm32l0xx.h"
#include "para.h"
#include "ds18b20.h"
#include "algorithm.h"

#include "string.h"


#define THR(x) (x - 2731) * 10000  
#define TENM_FIL_NUM    10
extern UserTypeDef UserPara;

IWDG_HandleTypeDef  IWDG_HandleStructure;

//uint32_t TemValue;
uint32_t ReadDataStartTime;
FlagStatus ReadDataFlag = RESET;
FlagStatus StaChangeFlag = RESET;
int TemFilterBuf[TENM_FIL_NUM];
int TemFilterBufBak[TENM_FIL_NUM];
extern FlagStatus UartRecvFlag;

//ϵͳĬ��ʱ��ΪMSI��ƵΪ2M���˴�����ʱ��ΪHSI16M(L031��ʱ�����Ϊ16M)
static void SystemClock_Config(void)
{
    RCC_ClkInitTypeDef RCC_ClkInitStruct;

    __HAL_RCC_HSI_CONFIG(RCC_HSI_ON);
    __HAL_RCC_HSI_CALIBRATIONVALUE_ADJUST(RCC_HSICALIBRATION_DEFAULT);
    
    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV4;                          
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;  
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;  
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0);
}


//���Ź�ʱ��ΪLSI��32.768K��8��Ƶ������ԼΪ4K
static void User_Iwdg_Init(void)
{
    IWDG_HandleStructure.Init.Prescaler = IWDG_PRESCALER_8;
    IWDG_HandleStructure.Init.Reload = 0x0FA0;                                  //��װ��ֵΪ4000��ԼΪ1S
    IWDG_HandleStructure.Init.Window = 0x0FA0;
    IWDG_HandleStructure.Instance = IWDG;
    HAL_IWDG_Init(&IWDG_HandleStructure);
}


static void User_Iwdg_Feed(void)
{
    HAL_IWDG_Refresh(&IWDG_HandleStructure);
}



void SysTick_Handler(void)
{
    HAL_IncTick();
}

 

void TemSta_Handle(void)
{
    if(ReadDataFlag == RESET)
    {
        ReadDataStartTime = HAL_GetTick();
    }
    else
    {
        if(HAL_GetTick() - ReadDataStartTime >= 160)
        { 
            ReadDataFlag = RESET;
            StaChangeFlag = RESET;
        }
    }   
}

 
static void TemData_Handle(void)
{
    static uint32_t TemCountCnt = 0;                                           //�����ʼ��Ϊ0
    static uint32_t ReadTemCnt = 0; 
    static FlagStatus FillBufStatus = RESET;
    static uint8_t temcnt=0;
    int PreTemp;

    //Disable_Lpuart1();                                                        //���¶�ʱ��ر����жϣ���ʱ�������ݽ������ܻᵼ�´�������
    PreTemp = DS18B20_Get_Temp();                                               //ÿ���һ���¶�ֵ
    //Enable_Lpuart1();
    
    if(ReadTemCnt++ <= 2)                                                      //ǰ3s�����ݲ�Ҫ����Ϊ�������85�ȣ�
    {
        return;
    }
        
    if(PreTemp == -27310000)                                                    //��ʼ��18B20���ɹ�,�ϵ�����85��������ȥ��    
    {
        DS18B20_Init();                                                         //���ֹ���ʱ��λһ������
        ReadTemCnt = 0;
        return;
    }
    
    if((PreTemp < -5000000) || (PreTemp > 13000000))                            //�¶Ȳ���-50~130��Χ�ڶ���
    {
        return;
    }
    if(UartRecvFlag == RESET)                                                   //�ڽ������ݵĹ����л���������ж�
    {
        UserPara.Temp = PreTemp;                                                //Ӱ������ݵ�׼ȷ��,�����ڽ������ݵĹ����ж�ȡ�����ݶ���
    }
    if(FillBufStatus == RESET)                                                  //��ʼ��ʮ����õ��ȶ������ٴ򿪴���
    {
        TemFilterBuf[temcnt++] = UserPara.Temp;
        if(temcnt >= TENM_FIL_NUM)
        {
            memcpy((uint8_t*)TemFilterBufBak, (uint8_t*)TemFilterBuf, TENM_FIL_NUM * 4);
            UserPara.Temp = (int)GetDelExtremeAndAverage(TemFilterBufBak, TENM_FIL_NUM, TENM_FIL_NUM / 3u, TENM_FIL_NUM / 3u); 
            Enable_Lpuart1();
            FillBufStatus = SET;
        }
        else
        {
            return;
        }
    }
    memcpy((uint8_t*)TemFilterBuf, (uint8_t*)TemFilterBuf + 4, (TENM_FIL_NUM - 1) * 4);
    *(TemFilterBuf + (TENM_FIL_NUM - 1)) = UserPara.Temp;                       //�õ�����ֵ���˲�
    memcpy((uint8_t*)TemFilterBufBak, (uint8_t*)TemFilterBuf, TENM_FIL_NUM * 4);
    UserPara.Temp = (int)GetDelExtremeAndAverage(TemFilterBufBak, TENM_FIL_NUM, TENM_FIL_NUM / 3u, TENM_FIL_NUM / 3u);    
    
    if(UserPara.Temp >  THR(UserPara.Up_Thr))                                   //�������ֵ
    {
        if(!(UserPara.AlarmSta & 0x00010000))                                   //������״̬��ת�����ڸ�ֵ
        {
            if(TemCountCnt++ >= UserPara.Du_Thr - 1)                            //�������������ֵʱ��
            {         
                TemCountCnt = 0;
                UserPara.AlarmSta |= 0x00010000;                                //����
                UserPara.AlarmSta &= 0x00010000;
            }
        }
        else                                                                    //����ʱ��+1s
        {
            TemCountCnt = 0;                                                    //�������ֵ��������������ֵ���¿�ʼ
            UserPara.Duration += 1;
        }
    }
    else if(UserPara.Temp < THR(UserPara.Do_Thr))                               //�������ֵ
    {
        if(!(UserPara.AlarmSta & 0x00000001))                                   //������״̬��ת�����ڵ�ֵ
        {
            if(TemCountCnt++ >= UserPara.Du_Thr - 1)                            //�������������ֵʱ��
            {
                TemCountCnt = 0;
                UserPara.AlarmSta |= 0x00000001;                                //����
                UserPara.AlarmSta &= 0x00000001;
            }
        }
        else                         
        {
            TemCountCnt = 0;                                                    //�������ֵ��������������ֵ���¿�ʼ
            UserPara.Duration += 1;                                             //����ʱ��+1s
        }
    }
    else                                                                        //����ֵ��Χ��
    {
        if(UserPara.AlarmSta != 0)                                              //�ӱ���״̬�л�������״̬
        {
            UserPara.Duration += 1;                                             //��Ҫ����+1s
            if(TemCountCnt++ >= UserPara.Du_Thr - 1)                            //�������������ֵʱ��
            {
                TemCountCnt = 0;
                UserPara.AlarmSta = 0;
                UserPara.Duration = 0;
            }
        }
        else
        {
            TemCountCnt = 0;                                                    //�������ֵ��������������ֵ���¿�ʼ
            UserPara.AlarmSta = 0;
            UserPara.Duration = 0;
        }
    }    
}




void main(void)
{ 
    uint32_t Alarm_Pre_Sta, Alarm_Cur_Sta;                                      //�����ϴ�״̬�ͱ���״̬
    uint32_t StartTime;
    
    SystemClock_Config();                                                       //ϵͳʱ�ӳ�ʼ��Ϊ16M
    HAL_InitTick(1);                                                            //�δ�ʱ�ӳ�ʼ��
    Uart_Config_Init();                                                         //���ڳ�ʼ��
    ReadPara();                                                                 //��ʼ������
    DS18B20_Init();                                                             //��ʼ��18B20 IO
   // User_Iwdg_Init();                                                           //��ʼ���������Ź�
    Disable_Lpuart1();                                                          //�ȹرմ��ڣ��������ȶ��ٴ�
    while(1)
    {
      //  User_Iwdg_Feed();                                                       //ι��
        TemSta_Handle();
        Alarm_Pre_Sta = UserPara.AlarmSta;                                      //����ϴεı���״̬
        MBASC_Function();                                                       //modbus����
        if(HAL_GetTick() - StartTime >= 100)
        {
            TemData_Handle();
            Alarm_Cur_Sta = UserPara.AlarmSta;                                  //��ñ��α���״̬
            if(Alarm_Cur_Sta != Alarm_Pre_Sta)                                  //���α���״̬����ͬ������״̬�仯
            {
                StaChangeFlag = SET;
            }
            StartTime = HAL_GetTick();            
        }
        if(HAL_GetTick() - StartTime >= 1000)
        {
             //UART1_SendData        
        }
    }
}





