/**
  ******************************************************************************
  * @file    hw_config.h
  * @brief   硬件配置头文件 — TR1-A1 占位版本
  *
  *          本文件是 TR1-A1 阶段的最小占位，仅满足 usb_lib.h 的
  *          #include "hw_config.h" 不报错。正式的时钟/GPIO/NVIC
  *          初始化接口在 TR1-A2 阶段填充。
  *
  *          ST USB 库的 usb_lib.h 在第 44 行硬性引用本文件，
  *          这是库与应用层的耦合点，无法绕过，故 A1 阶段必须
  *          提供本占位。
  ******************************************************************************
  */

#ifndef __HW_CONFIG_H
#define __HW_CONFIG_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x.h"
/*
 * ST USB 库的 usb_lib.h 第一个 #include 就是 hw_config.h，
 * 而 usb_regs.h 直接使用 __IO / uint16_t / uint8_t 等 CMSIS 类型
 * 却不自行包含 stm32f10x.h。因此 hw_config.h 必须作为 USB 库的
 * 类型依赖入口，率先引入 stm32f10x.h，否则 usb_regs.h 解析失败。
 */

/* TR1-A2 将在此添加以下内容：
  *   - Set_System()            系统时钟/GPIO/NVIC 初始化
  *   - Set_USBClock()          USB 48MHz 时钟使能
  *   - USB_Interrupts_Config() USB 中断配置
  *   - USB_Cable_Config()      软件重连 (PA12 切换)
  *   - void Enter_LowPowerMode(void)
  *   - void Leave_LowPowerMode(void)
  */

#endif /* __HW_CONFIG_H */
