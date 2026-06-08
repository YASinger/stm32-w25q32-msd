/**
  ******************************************************************************
  * @file    usb_conf.h
  * @brief   USB 配置头文件（最小版本）
  *           USB 核心库通过 usb_type.h 引用此文件。
  ******************************************************************************
  */

#ifndef __USB_CONF_H
#define __USB_CONF_H

/* 中断屏蔽寄存器 — usb_sil.c 需要此宏（CNTR_* 由 usb_regs.h 定义）-------- */
#define IMR_MSK (CNTR_CTRM  | CNTR_WKUPM | CNTR_SUSPM | CNTR_ERRM  | CNTR_SOFM \
                 | CNTR_ESOFM | CNTR_RESETM )

#endif /* __USB_CONF_H */
