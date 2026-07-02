/**
  ******************************************************************************
  * @file    usb_conf.h
  * @brief   USB 配置头文件
  ******************************************************************************
  */

#ifndef __USB_CONF_H
#define __USB_CONF_H

/* ── 端点数量 ────────────────────────────────────────────────────────────── */
#define EP_NUM              (3)         /* EP0 + EP1(IN) + EP2(OUT) */

/* ── 缓冲区描述表及端点缓冲区地址 (PMA 字偏移, 1 字 = 2 字节) ─────────── */
/*
 * PMA 布局 (512 字节 = 256 字, 0x40006000 ~ 0x400061FF)：
 *   注意: 以下地址均为 16-bit 字偏移, 库函数内部会 ×2 转为字节地址
 *
 *   0x00   BTABLE         32 字 = 64B  (端点 0~7 的 Buffer Description Table)
 *   0x20   ENDP0_RXADDR   32 字 = 64B  (EP0 接收)
 *   0x40   ENDP0_TXADDR   32 字 = 64B  (EP0 发送)
 *   0x60   ENDP1_TXADDR   32 字 = 64B  (EP1 Bulk IN)
 *   0x80   ENDP2_RXADDR   32 字 = 64B  (EP2 Bulk OUT)
 *   0xA0   (96 字 = 192 字节空闲, 到 PMA 末尾 0xFF)
 */

#define BTABLE_ADDRESS      (0x00)

#define ENDP0_RXADDR        (0x20)
#define ENDP0_TXADDR        (0x40)

#define ENDP1_TXADDR        (0x60)

#define ENDP2_RXADDR        (0x80)

/* ── 中断屏蔽寄存器 ──────────────────────────────────────────────────────── */
#define IMR_MSK (CNTR_CTRM  | CNTR_WKUPM | CNTR_SUSPM | CNTR_ERRM  | CNTR_SOFM \
                 | CNTR_ESOFM | CNTR_RESETM )

/* ── CTR 中断回调绑定 ────────────────────────────────────────────────────── */
/* TR1 阶段 EP1/EP2 不需要真正工作，全部指向 NOP_Process */
#define  EP1_IN_Callback   NOP_Process
#define  EP2_IN_Callback   NOP_Process
#define  EP3_IN_Callback   NOP_Process
#define  EP4_IN_Callback   NOP_Process
#define  EP5_IN_Callback   NOP_Process
#define  EP6_IN_Callback   NOP_Process
#define  EP7_IN_Callback   NOP_Process

#define  EP1_OUT_Callback  NOP_Process
#define  EP2_OUT_Callback  NOP_Process
#define  EP3_OUT_Callback  NOP_Process
#define  EP4_OUT_Callback  NOP_Process
#define  EP5_OUT_Callback  NOP_Process
#define  EP6_OUT_Callback  NOP_Process
#define  EP7_OUT_Callback  NOP_Process

#endif /* __USB_CONF_H */
