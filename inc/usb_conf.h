/**
  ******************************************************************************
  * @file    usb_conf.h
  * @brief   USB 配置头文件 — TR1-A1 占位版本
  *
  *          本文件是 TR1-A1 阶段的最小占位，仅满足 usb_type.h 的
  *          #include "usb_conf.h" 不报错。正式的端点数、PMA 地址、
  *          回调绑定等内容在 TR1-A3 阶段填充。
  ******************************************************************************
  */

#ifndef __USB_CONF_H
#define __USB_CONF_H

/*-------------------------------------------------------------*/
/* ISTR events  (A1 阶段硬依赖, 提前从 A3 引入)                  */
/*-------------------------------------------------------------*/
/*
 * usb_sil.c:73 (USB_SIL_Init) 直接引用 IMR_MSK 设置 CNTR 中断屏蔽,
 * 这是 USB 库的编译硬依赖, A1 必须提供, 否则 usb_sil.c 编译失败。
 *
 * 掩码项遵循 TR1 框架设计 §6.2 "禁用挂起模式" 的约束:
 *   - 包含 CNTR_SUSPM: 收到 SUSP 中断后由软件处理状态 (不进 STOP)
 *   - 包含 CNTR_WKUPM: 唤醒中断位 (虽然 TR1 禁用 WakeUp NVIC, 但掩码
 *                     保留以匹配库的预期; 真正禁用靠 NVIC 层)
 *
 * 各 CNTR_xxxM 位定义来自 usb_regs.h (经 usb_lib.h 间接包含)。
 */
#define IMR_MSK (CNTR_CTRM  | CNTR_WKUPM | CNTR_SUSPM | CNTR_ERRM  | CNTR_SOFM \
                 | CNTR_ESOFM | CNTR_RESETM )

/* TR1-A3 将在此添加以下内容：
  *   - EP_NUM          端点数量
  *   - BTABLE_ADDRESS  BTABLE 偏移
  *   - ENDP0_RXADDR    EP0 接收缓冲区地址
  *   - ENDP0_TXADDR    EP0 发送缓冲区地址
  *   - ENDP1_TXADDR    EP1 Bulk IN 发送地址
  *   - ENDP2_RXADDR    EP2 Bulk OUT 接收地址
  *   - EP1_IN_Callback ... EP7_OUT_Callback  端点回调宏
  */

#endif /* __USB_CONF_H */
