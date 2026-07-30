/**
  ******************************************************************************
  * @file    usb_istr.h
  * @brief   USB 中断服务头文件 — TR1-A4 (TR2-A3 增补 EP 回调声明)
  ******************************************************************************
  */

#ifndef __USB_ISTR_H
#define __USB_ISTR_H

void USB_Istr(void);

/*
 * TR2-A3: usb_conf.h 中 EP1_IN_Callback / EP2_OUT_Callback 的 NOP_Process
 * 宏已注释，改由 usb_endp.c 提供真实函数定义。此处补入函数原型，供
 * usb_istr.c 的 pEpInt_IN / pEpInt_OUT 数组在编译期识别这两个符号。
 *
 * 其余 12 个 EP 回调仍在 usb_conf.h 中宏定义为 NOP_Process，无需声明。
 * 后续若有更多端点接管真实回调，在此追加原型即可。
 */
void EP1_IN_Callback(void);
void EP2_OUT_Callback(void);

#endif /* __USB_ISTR_H */
