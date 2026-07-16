#ifndef __HW_CONFIG_H
#define __HW_CONFIG_H

#include "stm32f10x.h"

void Set_System(void);
void Set_USBClock(void);
void USB_Interrupts_Config(void);
void USB_Cable_Config(FunctionalState NewState);
void Get_SerialNum(void);            /* 用 MCU 唯一 ID 填充序列号字符串 (TR1-04) */

#endif /* __HW_CONFIG_H */
