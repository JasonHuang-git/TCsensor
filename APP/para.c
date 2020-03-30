#include "para.h"
#include "type.h"
#include "eeprom.h"

UserTypeDef UserPara;
uint8_t Cur_Param[USER_DEFAULT_LEN];

static uint8_t User_Default_Param[USER_DEFAULT_LEN] =
{
    0x01,
    0x21,
    0xBF,0x0F,          //���ֽ���ǰ��4031�൱��130�棬�¶ȱ�������ֵ
    0x47,0x0A,          //���ֽ���ǰ��2231�൱��-50�棬�¶ȱ�������ֵ        
    0xB7,0x08,          //���ֽ���ǰ��30s������ʱ����ֵ         
};

void ReadPara(void)
{
    uint8_t ParaInitFlag;
    Eeprom_ReadNBytes(RUN_ADDR_BASE, &ParaInitFlag, 1);
    if(ParaInitFlag != User_Default_Param[0])
    {
        Eeprom_WriteNBytes(RUN_ADDR_BASE, User_Default_Param, USER_DEFAULT_LEN);
    }
    Eeprom_ReadNBytes(RUN_ADDR_BASE, Cur_Param, USER_DEFAULT_LEN);
    UserPara.SlaveAddr = Cur_Param[1];
    UserPara.Up_Thr = ((uint16_t)Cur_Param[3] << 8) +Cur_Param[2];
    UserPara.Do_Thr = ((uint16_t)Cur_Param[5] << 8) +Cur_Param[4];
    UserPara.Du_Thr = ((uint16_t)Cur_Param[7] << 8) +Cur_Param[6];
}