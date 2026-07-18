/**
  ******************************************************************************
  * @file    usb_globals.c
  * @brief   USB 库全局符号占位 — TR1-A1 链接修复
  *
  *          ST USB 库的 usb_core.c / usb_init.c 在链接阶段
  *          引用 3 个全局符号，按 TR1 框架设计它们本应在 B2
  *          (usb_prop.c) 阶段定义。但 A1 的验收标准是"链接通过"，
  *          因此本文件提供 weak 占位定义，让 A1 闭环。
  *
  *          TR1-A4 已完成：wIstr / pEpInt_IN / pEpInt_OUT 三个
  *          符号已由 usb_istr.c 强定义覆盖，从本文件移除。
  *
  *          weak 属性确保 B2 正式实现时无需删除本文件 —— 链接器
  *          会自动选择 usb_prop.c 中的强定义覆盖此处。
  *          B2 完成后可直接从工程中移除本文件。
  ******************************************************************************
  */

#include "stm32f10x.h"
#include "usb_lib.h"

/*==== usb_prop.c 的符号 (TR1-B2 将正式定义) ==============================*/

/* 设备表 — usb_core.c 读取 Total_Endpoint / Total_Configuration */
DEVICE Device_Table __attribute__((weak)) = { 0, 0 };

/* 设备属性表 — usb_init.c 取其地址赋给 pProperty；
   usb_core.c 读取 MaxPacketSize 等字段 */
DEVICE_PROP Device_Property __attribute__((weak)) = { 0 };

/* 用户标准请求回调表 — usb_init.c 取其地址赋给 pUser_Standard_Requests */
USER_STANDARD_REQUESTS User_Standard_Requests __attribute__((weak)) = { 0 };
