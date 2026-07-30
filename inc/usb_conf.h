/**
  ******************************************************************************
  * @file    usb_conf.h
  * @brief   USB 配置头文件 — TR1-A3 正式版本
  *
  *          端点数 / PMA 缓冲区地址 / 端点回调绑定。
  *          IMR_MSK 已在 TR1-A1 补入（USB 库编译硬依赖）。
  ******************************************************************************
  */

#ifndef __USB_CONF_H
#define __USB_CONF_H

/*-------------------------------------------------------------*/
/* EP_NUM                                                      */
/* 设备用到的端点总数（含 EP0）                                  */
/*-------------------------------------------------------------*/
#define EP_NUM                          (3)

/*-------------------------------------------------------------*/
/* Buffer Description Table 与端点缓冲区地址                     */
/*                                                             */
/* 注意：以下为 16-bit 字偏移（库函数内部 ×2 转字节地址）。        */
/* 历史教训：v0.2.1 曾误用字节偏移导致与 BTABLE 重叠，枚举失败。   */
/* 布局见 TR1框架设计.md §6.3。                                 */
/*-------------------------------------------------------------*/

/* BTABLE 基地址（字偏移） */
#define BTABLE_ADDRESS      (0x00)

/* EP0 接收缓冲区（字偏移 0x20 = 字节 0x40，64B） */
#define ENDP0_RXADDR        (0x20)

/* EP0 发送缓冲区（字偏移 0x40 = 字节 0x80，64B） */
#define ENDP0_TXADDR        (0x40)

/* EP1 Bulk IN 发送缓冲区（字偏移 0x60 = 字节 0xC0，64B） */
#define ENDP1_TXADDR        (0x60)

/* EP2 Bulk OUT 接收缓冲区（字偏移 0x80 = 字节 0x100，64B） */
#define ENDP2_RXADDR        (0x80)

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

/*-------------------------------------------------------------*/
/* CTR service routines                                        */
/* 端点传输完成回调。                                            */
/*                                                             */
/* TR1 阶段全部指向 NOP_Process（空函数）。                       */
/* TR2-A3 起 EP1_IN / EP2_OUT 改由 usb_endp.c 提供真实 BOT 回调：*/
/*   - EP1_IN_Callback  -> Mass_Storage_In()  (CSW/数据 IN 完成) */
/*   - EP2_OUT_Callback -> Mass_Storage_Out() (CBW/数据 OUT 接收) */
/* 此处注释掉对应宏，链接器改用 usb_endp.c 中的函数定义。          */
/*-------------------------------------------------------------*/
//#define  EP1_IN_Callback   NOP_Process   /* TR2-A3: 替换为 usb_endp.c 中的真实函数 */
#define  EP2_IN_Callback   NOP_Process
#define  EP3_IN_Callback   NOP_Process
#define  EP4_IN_Callback   NOP_Process
#define  EP5_IN_Callback   NOP_Process
#define  EP6_IN_Callback   NOP_Process
#define  EP7_IN_Callback   NOP_Process

#define  EP1_OUT_Callback  NOP_Process
//#define  EP2_OUT_Callback  NOP_Process   /* TR2-A3: 替换为 usb_endp.c 中的真实函数 */
#define  EP3_OUT_Callback  NOP_Process
#define  EP4_OUT_Callback  NOP_Process
#define  EP5_OUT_Callback  NOP_Process
#define  EP6_OUT_Callback  NOP_Process
#define  EP7_OUT_Callback  NOP_Process

#endif /* __USB_CONF_H */
