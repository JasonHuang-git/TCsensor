#ifndef _TYPE_H
#define _TYPE_H

#include "stm32l0xx.h"



typedef struct 
{
    int Temp;           //�¶�ֵ*10000 
    uint8_t SlaveAddr;
    //uint8_t SenSta;     //������״̬  �ò���      
    uint16_t Up_Thr;    //�¶��Ϸ�ֵ  
    uint16_t Do_Thr;    //�¶��·�ֵ
    uint16_t Du_Thr;    //����ʱ�䷧ֵ������������ʱ��Ž���״̬�л���   
    uint32_t Duration;  //���������¶���ֵ����ʱ��
    uint32_t AlarmSta;  //����״̬
}UserTypeDef;




#endif