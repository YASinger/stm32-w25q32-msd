/**
  ******************************************************************************
  * @file    usb_bot.c
  * @brief   BOT (Bulk-Only Transport) 状态机实现
  ******************************************************************************
  */

#include "usb_lib.h"
#include "usb_bot.h"
#include "usb_conf.h"
#include "usb_regs.h"
#include "usb_mem.h"
#include "usb_scsi.h"
#include "mass_mal.h"

/* ── 全局变量 ─────────────────────────────────────────────────────────────── */
uint8_t  Bot_State;                               /* 当前状态机状态 */
uint8_t  Bulk_Data_Buff[BULK_MAX_PACKET_SIZE];     /* EP2 OUT 收发缓冲区 (64B) */
uint16_t Data_Len;                                /* 本次 EP2 OUT 收到的字节数 */
Bulk_Only_CBW CBW;                                 /* 当前命令包 */
Bulk_Only_CSW CSW;                                 /* 待发送的状态包 */
uint32_t SCSI_LBA;                                 /* READ10/WRITE10 起始扇区号 */
uint32_t SCSI_BlkLen;                              /* READ10/WRITE10 扇区数 */

extern uint32_t Max_Lun;   /* 定义在 usb_prop.c, 值为 0 (仅 LUN 0) */

/* ═══════════════════════════════════════════════════════════════════════════
 *  EP1 IN 中断入口 — CSW 发送完成 / READ 数据继续发送
 * ═══════════════════════════════════════════════════════════════════════════ */
void Mass_Storage_In(void)
{
    switch (Bot_State) {
    case BOT_CSW_Send:
    case BOT_ERROR:
        /* CSW 已发完 (或错误状态已通知主机), 回到 IDLE 等下一个 CBW */
        Bot_State = BOT_IDLE;
        SetEPRxStatus(ENDP2, EP_RX_VALID);
        break;

    case BOT_DATA_IN:
        /* TR2-S3: READ10 继续发送下一包数据 */
        break;

    case BOT_DATA_IN_LAST:
        /* 最后一包数据已发完, 发送 CSW */
        Set_CSW(CSW_CMD_PASSED, SEND_CSW_ENABLE);
        SetEPRxStatus(ENDP2, EP_RX_VALID);
        break;

    default:
        break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  EP2 OUT 中断入口 — 接收 CBW / WRITE 数据
 * ═══════════════════════════════════════════════════════════════════════════ */
void Mass_Storage_Out(void)
{
    Data_Len = USB_SIL_Read(EP2_OUT, Bulk_Data_Buff);

    switch (Bot_State) {
    case BOT_IDLE:
        /* 首包: 解码 CBW */
        CBW_Decode();
        break;

    case BOT_DATA_OUT:
        /* TR2-S3: WRITE10 数据写入 */
        /* TR2-S1 阶段走错误路径 */
        Bot_Abort(DIR_OUT);
        Set_CSW(CSW_PHASE_ERROR, SEND_CSW_DISABLE);
        break;

    default:
        Bot_Abort(BOTH_DIR);
        Set_CSW(CSW_PHASE_ERROR, SEND_CSW_DISABLE);
        break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  CBW_Decode — 解码 CBW 并分发 SCSI 命令
 * ═══════════════════════════════════════════════════════════════════════════ */
void CBW_Decode(void)
{
    uint32_t Counter;

    /* 将收到的数据拷贝到 CBW 结构体 */
    for (Counter = 0; Counter < Data_Len; Counter++) {
        *((uint8_t *)&CBW + Counter) = Bulk_Data_Buff[Counter];
    }

    /* CSW 回填 Tag 和初始残留量 */
    CSW.dTag = CBW.dTag;
    CSW.dDataResidue = CBW.dDataLength;

    /* 长度校验: CBW 必须是 31 字节 */
    if (Data_Len != BOT_CBW_PACKET_LENGTH) {
        Bot_Abort(BOTH_DIR);
        CBW.dSignature = 0;     /* 清签名, 阻止 ClearFeature 直到收到 Reset */
        Set_CSW(CSW_CMD_FAILED, SEND_CSW_DISABLE);
        return;
    }

    /* 预解析 READ10/WRITE10 的 LBA 和块数 */
    if ((CBW.CB[0] == 0x28) || (CBW.CB[0] == 0x2A)) {   /* SCSI_READ10 / SCSI_WRITE10 */
        SCSI_LBA    = (CBW.CB[2] << 24) | (CBW.CB[3] << 16)
                    | (CBW.CB[4] <<  8) |  CBW.CB[5];
        SCSI_BlkLen = (CBW.CB[7] <<  8) |  CBW.CB[8];
    }

    /* 签名校验 */
    if (CBW.dSignature != BOT_CBW_SIGNATURE) {
        Bot_Abort(BOTH_DIR);
        Set_CSW(CSW_CMD_FAILED, SEND_CSW_DISABLE);
        return;
    }

    /* LUN / CB 长度合法性 */
    if ((CBW.bLUN > Max_Lun) || (CBW.bCBLength < 1) || (CBW.bCBLength > 16)) {
        Bot_Abort(BOTH_DIR);
        Set_CSW(CSW_CMD_FAILED, SEND_CSW_DISABLE);
        return;
    }

    /*
     * TR2-S2: SCSI 命令分发
     * 查询命令已实现, READ10/WRITE10 留到 TR2-S3
     */
    switch (CBW.CB[0]) {
    case SCSI_TEST_UNIT_READY:
        SCSI_TestUnitReady_Cmd(CBW.bLUN);
        break;
    case SCSI_REQUEST_SENSE:
        SCSI_RequestSense_Cmd(CBW.bLUN);
        break;
    case SCSI_INQUIRY:
        SCSI_Inquiry_Cmd(CBW.bLUN);
        break;
    case SCSI_START_STOP_UNIT:
        SCSI_Start_Stop_Unit_Cmd(CBW.bLUN);
        break;
    case SCSI_ALLOW_MEDIUM_REMOVAL:
        SCSI_Start_Stop_Unit_Cmd(CBW.bLUN);    /* 复用 */
        break;
    case SCSI_MODE_SENSE6:
        SCSI_ModeSense6_Cmd(CBW.bLUN);
        break;
    case SCSI_MODE_SENSE10:
        SCSI_ModeSense10_Cmd(CBW.bLUN);
        break;
    case SCSI_READ_FORMAT_CAPACITIES:
        SCSI_ReadFormatCapacity_Cmd(CBW.bLUN);
        break;
    case SCSI_READ_CAPACITY10:
        SCSI_ReadCapacity10_Cmd(CBW.bLUN);
        break;
    case SCSI_FORMAT_UNIT:
        SCSI_Start_Stop_Unit_Cmd(CBW.bLUN);    /* TR2-S2 暂复用, TR2-S3 实现 */
        break;

    /* TR2-S3 才实现 READ10/WRITE10/VERIFY10 */
    case SCSI_READ10:
    case SCSI_WRITE10:
    case SCSI_VERIFY10:
    default:
        Set_CSW(CSW_CMD_FAILED, SEND_CSW_ENABLE);
        break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Transfer_Data_Request — 发送数据到主机 (供 SCSI 命令处理函数调用)
 * ═══════════════════════════════════════════════════════════════════════════ */
void Transfer_Data_Request(uint8_t *Data_Pointer, uint16_t Data_Len)
{
    USB_SIL_Write(EP1_IN, Data_Pointer, Data_Len);
    SetEPTxStatus(ENDP1, EP_TX_VALID);

    Bot_State = BOT_DATA_IN_LAST;
    CSW.dDataResidue -= Data_Len;
    CSW.bStatus = CSW_CMD_PASSED;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Set_CSW — 填充并发送 CSW
 * ═══════════════════════════════════════════════════════════════════════════ */
void Set_CSW(uint8_t CSW_Status, uint8_t Send_Permission)
{
    CSW.dSignature = BOT_CSW_SIGNATURE;
    CSW.bStatus = CSW_Status;

    USB_SIL_Write(EP1_IN, (uint8_t *)&CSW, CSW_DATA_LENGTH);

    Bot_State = BOT_ERROR;     /* 默认进入错误态 */
    if (Send_Permission) {
        Bot_State = BOT_CSW_Send;
        SetEPTxStatus(ENDP1, EP_TX_VALID);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Bot_Abort — STALL 指定方向的端点
 * ═══════════════════════════════════════════════════════════════════════════ */
void Bot_Abort(uint8_t Direction)
{
    switch (Direction) {
    case DIR_IN:
        SetEPTxStatus(ENDP1, EP_TX_STALL);
        break;
    case DIR_OUT:
        SetEPRxStatus(ENDP2, EP_RX_STALL);
        break;
    case BOTH_DIR:
        SetEPTxStatus(ENDP1, EP_TX_STALL);
        SetEPRxStatus(ENDP2, EP_RX_STALL);
        break;
    default:
        break;
    }
}
