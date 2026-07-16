/**
  ******************************************************************************
  * @file    usb_bot.h
  * @brief   BOT (Bulk-Only Transport) 状态机 — CBW/CSW 结构体与接口
  ******************************************************************************
  */

#ifndef __USB_BOT_H
#define __USB_BOT_H

#include "stm32f10x.h"

/* ── CBW / CSW 结构体 (USB MSC Bulk-Only 规范) ───────────────────────────── */

/* Command Block Wrapper — 主机发给设备的命令包, 31 字节 */
typedef struct _Bulk_Only_CBW {
    uint32_t dSignature;    /* 固定 0x43425355 ("USBC") */
    uint32_t dTag;          /* 主机生成, CSW 必须回填相同值 */
    uint32_t dDataLength;   /* 数据阶段总字节数 */
    uint8_t  bmFlags;       /* bit7=0: OUT 数据, bit7=1: IN 数据 */
    uint8_t  bLUN;          /* 目标 LUN 号 */
    uint8_t  bCBLength;     /* SCSI 命令块长度 (1~16) */
    uint8_t  CB[16];        /* SCSI 命令块 */
} Bulk_Only_CBW;

/* Command Status Wrapper — 设备回给主机的状态包, 13 字节 */
typedef struct _Bulk_Only_CSW {
    uint32_t dSignature;    /* 固定 0x53425355 ("USBS") */
    uint32_t dTag;          /* 与 CBW.dTag 相同 */
    uint32_t dDataResidue;  /* 剩余未传输字节数 */
    uint8_t  bStatus;       /* 0=PASS, 1=FAIL, 2=PHASE_ERROR */
} Bulk_Only_CSW;

/* ── BOT 状态机状态 ──────────────────────────────────────────────────────── */
#define BOT_IDLE            0   /* 等待 CBW, EP2 OUT 接收就绪 */
#define BOT_DATA_OUT        1   /* 正在接收 WRITE 数据 */
#define BOT_DATA_IN         2   /* 正在发送 READ 数据 (还有后续包) */
#define BOT_DATA_IN_LAST    3   /* 最后一包 IN 数据已发, 准备发 CSW */
#define BOT_CSW_Send        4   /* 正在发送 CSW */
#define BOT_ERROR           5   /* 出错, 端点已 STALL, 等待 Bulk-Only Reset */

/* ── CBW / CSW 签名 ──────────────────────────────────────────────────────── */
#define BOT_CBW_SIGNATURE   0x43425355  /* "USBC" */
#define BOT_CSW_SIGNATURE   0x53425355  /* "USBS" */
#define BOT_CBW_PACKET_LENGTH 31         /* CBW 固定长度 */
#define CSW_DATA_LENGTH     0x000D       /* CSW 固定长度 13 字节 */

/* ── CSW 状态码 ───────────────────────────────────────────────────────────── */
#define CSW_CMD_PASSED      0x00
#define CSW_CMD_FAILED      0x01
#define CSW_PHASE_ERROR     0x02

/* ── Set_CSW 发送许可 ─────────────────────────────────────────────────────── */
#define SEND_CSW_DISABLE    0
#define SEND_CSW_ENABLE     1

/* ── Bot_Abort 方向 ───────────────────────────────────────────────────────── */
#define DIR_IN              0
#define DIR_OUT             1
#define BOTH_DIR            2

/* ── 函数声明 ──────────────────────────────────────────────────────────────── */
void Mass_Storage_In(void);
void Mass_Storage_Out(void);
void CBW_Decode(void);
void Transfer_Data_Request(uint8_t *Data_Pointer, uint16_t Data_Len);
void Set_CSW(uint8_t CSW_Status, uint8_t Send_Permission);
void Bot_Abort(uint8_t Direction);

/* ── 全局变量 extern 声明 (定义在 usb_bot.c, 供 usb_prop.c 使用) ─────────── */
extern uint8_t  Bot_State;
extern uint8_t  Bulk_Data_Buff[];
extern uint16_t Data_Len;
extern Bulk_Only_CBW CBW;
extern Bulk_Only_CSW CSW;
extern uint32_t SCSI_LBA;
extern uint32_t SCSI_BlkLen;

#endif /* __USB_BOT_H */
