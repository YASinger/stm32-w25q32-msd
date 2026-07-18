/**
  ******************************************************************************
  * @file    usb_globals.c
  * @brief   USB 库全局符号占位 — TR1-A1 链接修复
  *
  *          ST USB 库的 usb_core.c / usb_init.c / usb_int.c 在链接阶段
  *          引用 6 个全局符号，按 TR1 框架设计它们本应分别在 A4
  *          (usb_istr.c) 和 B2 (usb_prop.c) 阶段定义。但 A1 的验收
  *          标准是"链接通过"，因此本文件提供 weak 占位定义，让 A1
  *          闭环。
  *
  *          weak 属性确保 A4/B2 正式实现时无需删除本文件 —— 链接器
  *          会自动选择 usb_istr.c / usb_prop.c 中的强定义覆盖此处。
  *          届时可直接从工程中移除本文件，或保留（weak 会被忽略）。
  *
  *          占位值说明：
  *          - 结构体/数组清零：避免库代码访问野指针
  *          - 函数指针为 NULL：A1 不响应任何 USB 事件（符合"PC 不
  *            识别设备"的 A1 特征），仅保证链接通过
  ******************************************************************************
  */

#include "stm32f10x.h"
#include "usb_lib.h"

/*==== usb_istr.c 的符号 (TR1-A4 将正式定义) ==============================*/

/* ISTR 寄存器最近一次读取值 — usb_int.c 读写它 */
__IO uint16_t wIstr __attribute__((weak)) = 0;

/* 端点 IN/OUT 中断回调表 — usb_int.c 通过函数指针调用 */
/* 初始化为全 NULL，A1 阶段端点中断不会触发（未 PowerOn） */
void (*pEpInt_IN[7])(void)  __attribute__((weak)) = {0};
void (*pEpInt_OUT[7])(void) __attribute__((weak)) = {0};


/*==== usb_prop.c 的符号 (TR1-B2 将正式定义) ==============================*/

/* 设备表 — usb_core.c 读取 Total_Endpoint / Total_Configuration */
DEVICE Device_Table __attribute__((weak)) = { 0, 0 };

/* 设备属性表 — usb_init.c 取其地址赋给 pProperty；
   usb_core.c 读取 MaxPacketSize 等字段 */
DEVICE_PROP Device_Property __attribute__((weak)) = { 0 };

/* 用户标准请求回调表 — usb_init.c 取其地址赋给 pUser_Standard_Requests */
USER_STANDARD_REQUESTS User_Standard_Requests __attribute__((weak)) = { 0 };
