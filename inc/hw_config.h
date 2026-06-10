#ifndef __HW_CONFIG_H
#define __HW_CONFIG_H

#include "stm32f10x.h"

void Set_System(void);
void Set_USBClock(void);
void USB_Interrupts_Config(void);
void USB_Cable_Config(FunctionalState NewState);

#endif /* __HW_CONFIG_H */
