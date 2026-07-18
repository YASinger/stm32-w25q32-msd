/**
  ******************************************************************************
  * @file    hw_config.h
  * @brief   硬件配置头文件 — TR1-A2 正式版本
  *
  *          ST USB 库的 usb_lib.h 在第 44 行硬性引用本文件，
  *          这是库与应用层的耦合点，无法绕过。
  *
  *          本文件是 USB 库的类型依赖入口：usb_regs.h 直接使用
  *          __IO / uint16_t 等 CMSIS 类型却不自包含，依赖本文件
  *          率先引入 stm32f10x.h。
  ******************************************************************************
  */

#ifndef __HW_CONFIG_H
#define __HW_CONFIG_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x.h"

/* Exported functions --------------------------------------------------------*/
void Set_System(void);
void Set_USBClock(void);
void USB_Interrupts_Config(void);
void USB_Cable_Config(FunctionalState NewState);
void Enter_LowPowerMode(void);
void Leave_LowPowerMode(void);
void Get_SerialNum(void);            /* C2: 用 MCU 唯一 ID 填充序列号字符串 */

#endif /* __HW_CONFIG_H */
