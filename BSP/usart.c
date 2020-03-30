#include <stdarg.h>
#include <string.h>
#include "type.h"
#include "usart.h"
#include "modbus_ascii.h"

#define LPUART1_ENABLE_IT(__HANDLE__, __INTERRUPT__) (((((uint8_t)(__INTERRUPT__)) >> 5U) == 1U)? ((__HANDLE__)->CR1 |= (1U << ((__INTERRUPT__) & UART_IT_MASK))): \
                                                           ((((uint8_t)(__INTERRUPT__)) >> 5U) == 2U)? ((__HANDLE__)->CR2 |= (1U << ((__INTERRUPT__) & UART_IT_MASK))): \
                                                           ((__HANDLE__)->CR3 |= (1U << ((__INTERRUPT__) & UART_IT_MASK))))
#define LPUART1_DISABLE_IT(__HANDLE__, __INTERRUPT__) (((((uint8_t)(__INTERRUPT__)) >> 5U) == 1U)? ((__HANDLE__)->CR1 &= ~ (1U << ((__INTERRUPT__) & UART_IT_MASK))): \
                                                           ((((uint8_t)(__INTERRUPT__)) >> 5U) == 2U)? ((__HANDLE__)->CR2 &= ~ (1U << ((__INTERRUPT__) & UART_IT_MASK))): \
                                                           ((__HANDLE__)->CR3 &= ~ (1U << ((__INTERRUPT__) & UART_IT_MASK))))


uint8_t u8SendNum = 0;                  //��ǰ�������ݸ���
uint8_t u8SendIndex = 0;                //�������ݻ���ǰ׺
uint8_t UART1_RXBuffLen = 0;            //�������ݻ��泤��

FlagStatus UartRecvFlag = RESET;
FlagStatus UartRecvFrameOK = RESET;  //����������ɱ�־
uint8_t UART1_RXBuff[USART1_MAX_DATALEN];//�������ݻ���
uint8_t UART1_TXBuff[USART1_MAX_DATALEN];//�������ݻ���


//****************************************************************************************************************************************************************
// ����               : Usart_Gpio_Init()
// ��������           : 2017-11-27
// ����               : ÷����
// ����               : ͨ�ô����շ���������
// �������           : ��
// �������           : ��
// ���ؽ��           : ��
// ע���˵��         : ��
// �޸�����           :
//****************************************************************************************************************************************************************
void Usart_Gpio_Init(void)
{
    SET_BIT(RCC->IOPENR, RCC_IOPENR_GPIOAEN);                                   //GPIOA��ʱ��
    EN_485_PORT->MODER &= 0xFFFFFCFF;            
    EN_485_PORT->MODER |= 0x00000100;                                           //PA4ͨ�����ģʽ
    EN_485_PORT->OTYPER &= ~(1<<4);                                             //�������
    EN_485_PORT->OSPEEDR |= (3<<8);                                             //����  
    EN_485_PORT->BRR = EN_485_PIN;                                              //����״̬Ϊ��
    
    LPUSART1_PORT->MODER &= 0xFFFFFF0F;            
    LPUSART1_PORT->MODER |= 0x000000A0;                                         //PA2 PA3���ù���ģʽ
    LPUSART1_PORT->OTYPER &= ~(3<<2);                                           //�������
    LPUSART1_PORT->AFR[0] &= 0xFFFF00FF;                                        //AF6�����ֲ�
    LPUSART1_PORT->AFR[0] |= 0x00006600;
}






//****************************************************************************************************************************************************************
// ����               : Usart_Config_Init()
// ��������           : 2017-11-27
// ����               : ÷����
// ����               : ͨ�ô��ڲ�����ʼ��
// �������           : UARTx_ConfigTypeDef(����ͨ�Ų���)
// �������           : ��
// ���ؽ��           : ��
// ע���˵��         : ��
// �޸�����           :
//****************************************************************************************************************************************************************
void Uart_Config_Init(void)
{  
    Usart_Gpio_Init(); 
      
    SET_BIT(RCC->APB1ENR, (RCC_APB1ENR_LPUART1EN));                             //��LPUSART1ʱ�� 
    NVIC->IP[_IP_IDX(LPUART1_IRQn)] = (NVIC->IP[_IP_IDX(LPUART1_IRQn)] & ~(0xFF << _BIT_SHIFT(LPUART1_IRQn))) |
       (((1 << (8 - __NVIC_PRIO_BITS)) & 0xFF) << _BIT_SHIFT(LPUART1_IRQn));    //���������ȼ�Ϊ1
    NVIC->ISER[0] = (1 << ((uint32_t)(LPUART1_IRQn) & 0x1F));                   //ʹ�����ж�
    
    LPUART1->CR1 &=  ~USART_CR1_UE;                                             //��ʧ�ܴ���
    MODIFY_REG(LPUART1->CR1, ((0x10001U << 12) | (1 << 10) | (1 << 9) | (1 << 3) | (1 << 2) | (1 << 15)), 
        UART_WORDLENGTH_8B | UART_PARITY_NONE | UART_MODE_TX_RX);               //��������λ��У��λ������ģʽ
    MODIFY_REG(LPUART1->CR2, USART_CR2_STOP, UART_STOPBITS_1);                  //����ֹͣλ
    MODIFY_REG(LPUART1->CR3, (USART_CR3_RTSE | USART_CR3_CTSE | USART_CR3_ONEBIT), UART_HWCONTROL_NONE);
    LPUART1->BRR = (uint32_t)(UART_DIV_LPUART(SystemCoreClock, 9600));          //���ò����ʣ�ϵͳʱ��16M��������9600��
    CLEAR_BIT(LPUART1->CR2, (USART_CR2_LINEN | USART_CR2_CLKEN));
    CLEAR_BIT(LPUART1->CR3, (USART_CR3_SCEN | USART_CR3_HDSEL | USART_CR3_IREN));
    LPUART1_ENABLE_IT(LPUART1, UART_IT_RXNE);                                   //ʹ�ܽ����ж�
    LPUART1->CR1 |=  USART_CR1_UE;                                              //ʹ�ܴ���
}





//******************************************************************************
// ����               : UART1_RecvData()
// ��������           : 2017-11-27
// ����               : ÷����
// ����               : UART1�������ݺ���
// �������           : uint8_t *UART1_RecvBuff �������ݻ���
//                      uint32_t Len            �������ݳ���        
// �������           : ��
// ���ؽ��           : ��
// ע���˵��         : 
// �޸�����           :
//******************************************************************************
int32_t UART1_RecvData(uint8_t *UART1_RecvBuff, uint32_t Len)
{
    uint32_t i = 0;

    if ((0 == Len) || (((uint8_t*)NULL) == UART1_RecvBuff))
    {
        return 0;
    }

    if ((RESET == UartRecvFrameOK) || (0 == UART1_RXBuffLen))
    {
        return 0;
    }

    if (Len < UART1_RXBuffLen)
    {
        return -1;
    }

    Len = UART1_RXBuffLen;

    for (i = 0; i < Len; i++)
    {
        UART1_RecvBuff[i] = UART1_RXBuff[i];
    }

    UART1_RXBuffLen = 0;

    return Len;
}


//******************************************************************************
// ����               : UART1_SendData()
// ��������           : 2017-11-27
// ����               : ÷����
// ����               : UART1�������ݺ���
// �������           : uint8_t *UART1_SendBuff �������ݻ���
//                      uint32_t Len            �������ݳ���        
// �������           : ��
// ���ؽ��           : ��
// ע���˵��         : 
// �޸�����           :
//******************************************************************************
/*
uint32_t UART1_SendData(uint8_t *UART1_SendBuff, uint32_t Len)
{
    uint32_t i = 0;

    if ((0 == Len) || (((uint8_t*)0) == UART1_SendBuff))
    {
        return 0;
    }
    if (u8SendNum != 0)
    {
        return 0;
    }

    if (Len > (sizeof(UART1_TXBuff) / sizeof(UART1_TXBuff[0])))
    {
        Len = (sizeof(UART1_TXBuff) / sizeof(UART1_TXBuff[0]));
    }

    for (i = 0; i < Len; i++)
    {
        UART1_TXBuff[i] = UART1_SendBuff[i];
    }
    TX_ON;
    LPUART1->TDR = UART1_TXBuff[0];
    u8SendIndex = 1;
    u8SendNum = Len;
    LPUART1_ENABLE_IT(LPUART1, UART_IT_TC);
    return Len;
}
*/
//******************************************************************************
// ����               : UART1_SendData()
// ��������           : 2018-10-09
// ����               : ÷����
// ����               : UART1�������ݺ���
// �������           : uint8_t *UART1_SendBuff �������ݻ���
//                      uint32_t Len            �������ݳ���        
// �������           : ��
// ���ؽ��           : ��
// ע���˵��         : ʹ���жϷ��ͣ��ڶ�ȡ�¶ȹر����ж�ʱ���ܻ����𴮿��ж��޷��ٿ���
// �޸�����           : 
//******************************************************************************
uint32_t UART1_SendData(uint8_t *buf, uint32_t len)
{
    uint8_t i;
    uint32_t TimeOutCount;
    
    if((0 == len) || (((uint8_t*)NULL) == buf) || (len > sizeof(UART1_TXBuff) / sizeof(UART1_TXBuff[0])))
    {
        return 0;
    }
    TX_ON;
    for(i = 0; i <= len; i++)                                                   //ע��i<=len,������ٷ����һ���ֽ�     
    {   
        TimeOutCount = 0;
        *(UART1_TXBuff + i) = *(buf + i);
        LPUART1->TDR = *(UART1_TXBuff + i);
        while((LPUART1->ISR & ((uint32_t)1U << (UART_IT_TXE >> 0x08U))) == RESET)
        {
            if(TimeOutCount++ > 0xffff)                                         //��ʱ
            {
                TX_OFF;
                TimeOutCount = 0;
                return 0;
            }
        }
    } 
    TX_OFF;
    return len;
}




void Enable_Lpuart1(void)
{
    //LPUART1_ENABLE_IT(LPUART1, UART_IT_TC);
    LPUART1_ENABLE_IT(LPUART1, UART_IT_RXNE);
    LPUART1->CR1 |=  USART_CR1_UE;
}


void Disable_Lpuart1(void)
{
    //LPUART1_DISABLE_IT(LPUART1, UART_IT_TC);
    LPUART1_DISABLE_IT(LPUART1, UART_IT_RXNE);   
    LPUART1->CR1 &=  ~USART_CR1_UE;
}

//******************************************************************************
// ����               : USART1_IRQHandler()
// ��������           : 2017-11-27
// ����               : ÷����
// ����               : UART1�����ж�
// �������           : ��
//                      ��       
// �������           : ��
// ���ؽ��           : ��
// ע���˵��         : 
// �޸�����           :
//*******************************************************************************
void LPUART1_IRQHandler(void)
{
    uint8_t RecvByteData;
    
    UartRecvFlag = SET;                                                         //�����˴����жϣ���ʼ���������ˣ���ʱ��ȡ���¶����ݶ���
    if((LPUART1->ISR & ((uint32_t)1U << (UART_IT_RXNE >> 0x08U))) != RESET)
    {
        LPUART1->ICR = UART_IT_RXNE;
        
        RecvByteData = LPUART1->RDR;
                  
        MODBUS_ASCII_HandlRevData(RecvByteData); //����ASCII����     
    }
/*      
    if((LPUART1->ISR & ((uint32_t)1U << (UART_IT_TC >> 0x08U))) != RESET)       //�жϷ������ڶ�ȡ�¶�ʱ��������
    {
        LPUART1->ICR = UART_IT_TC;

        if(u8SendIndex >= u8SendNum)
        {
            u8SendNum = 0;
            LPUART1_DISABLE_IT(LPUART1, UART_IT_TC);
            TX_OFF;
        }
        else
        {
            LPUART1->TDR = UART1_TXBuff[u8SendIndex++];
        }
    } */   
}


//**************************************************************************************************
// ����         : uprintf(const char *fmt,...)
// ��������     : 2016-09-08
// ����         : ÷����
// ����         : �򴮿ڷ���ָ����ʽ������
// �������     : �룺fmt,...    ��������ָ��
// �������     : ��
// ���ؽ��     : ��
// ע���˵��   : ��usart��ӡ���ڵ����ݣ����ڵ���
// �޸�����     : 
//**************************************************************************************************

void uprintf(const char *fmt,...)
{
    va_list marker;
    char buff[32];
    memset(buff,0,sizeof(buff));
    va_start(marker, fmt);
    vsprintf(buff,fmt,marker);
    va_end(marker);
    UART1_SendData((uint8_t*)buff, strlen(buff));
    while(u8SendNum!=0);    
}

