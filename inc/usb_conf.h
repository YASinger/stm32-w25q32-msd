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

/* ── 缓冲区描述表及端点缓冲区地址 (PMA 偏移) ──────────────────────────────── */
/*
 * PMA 布局 (512 字节, 0x40006000 ~ 0x400061FF)：
 *
 *   0x000  BTABLE         8 × 8B = 64B   (端点 0~7 的 Buffer Description Table)
 *   0x018  ENDP0_RXADDR   64B
 *   0x058  ENDP0_TXADDR   64B
 *   0x098  ENDP1_TXADDR   64B
 *   0x0D8  ENDP2_RXADDR   64B
 *   0x118  (232 字节空闲)
 */

#define BTABLE_ADDRESS      (0x00)

#define ENDP0_RXADDR        (0x18)
#define ENDP0_TXADDR        (0x58)

#define ENDP1_TXADDR        (0x98)

#define ENDP2_RXADDR        (0xD8)

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
