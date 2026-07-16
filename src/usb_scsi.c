/**
  ******************************************************************************
  * @file    usb_scsi.c
  * @brief   SCSI 查询命令处理 (TR2-S2)
  ******************************************************************************
  */

#include "usb_lib.h"
#include "usb_scsi.h"
#include "usb_bot.h"
#include "mass_mal.h"

/* ── extern 引用 BOT 全局变量 (定义在 usb_bot.c) ─────────────────────────── */
extern Bulk_Only_CBW CBW;
extern Bulk_Only_CSW CSW;
extern uint32_t Mass_Memory_Size[2];
extern uint32_t Mass_Block_Size[2];
extern uint32_t Mass_Block_Count[2];

/* ═══════════════════════════════════════════════════════════════════════════
 *  SCSI_Inquiry_Cmd — 返回设备信息 (36 字节或 VPD 页)
 * ═══════════════════════════════════════════════════════════════════════════ */
void SCSI_Inquiry_Cmd(uint8_t lun)
{
    uint8_t *Inquiry_Data;
    uint16_t Inquiry_Data_Length;

    if (CBW.CB[1] & 0x01) {
        /* Evpd=1: 请求 VPD 页 */
        Inquiry_Data = Page00_Inquiry_Data;
        Inquiry_Data_Length = 5;
    } else {
        /* Evpd=0: 标准 INQUIRY */
        Inquiry_Data = Standard_Inquiry_Data;
        if (CBW.CB[4] <= STANDARD_INQUIRY_DATA_LEN)
            Inquiry_Data_Length = CBW.CB[4];
        else
            Inquiry_Data_Length = STANDARD_INQUIRY_DATA_LEN;
    }

    Transfer_Data_Request(Inquiry_Data, Inquiry_Data_Length);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  SCSI_ReadFormatCapacity_Cmd — 返回格式化容量信息 (12 字节)
 * ═══════════════════════════════════════════════════════════════════════════ */
void SCSI_ReadFormatCapacity_Cmd(uint8_t lun)
{
    if (MAL_GetStatus(lun) != 0) {
        Set_Scsi_Sense_Data(CBW.bLUN, NOT_READY, MEDIUM_NOT_PRESENT);
        Set_CSW(CSW_CMD_FAILED, SEND_CSW_ENABLE);
        Bot_Abort(DIR_IN);
        return;
    }

    /* 填充 Block Count (大端) */
    ReadFormatCapacity_Data[4] = (uint8_t)(Mass_Block_Count[lun] >> 24);
    ReadFormatCapacity_Data[5] = (uint8_t)(Mass_Block_Count[lun] >> 16);
    ReadFormatCapacity_Data[6] = (uint8_t)(Mass_Block_Count[lun] >>  8);
    ReadFormatCapacity_Data[7] = (uint8_t)(Mass_Block_Count[lun]);

    /* 填充 Block Length = 512 = 0x000200 (大端) */
    ReadFormatCapacity_Data[9]  = (uint8_t)(Mass_Block_Size[lun] >> 16);
    ReadFormatCapacity_Data[10] = (uint8_t)(Mass_Block_Size[lun] >>  8);
    ReadFormatCapacity_Data[11] = (uint8_t)(Mass_Block_Size[lun]);

    Transfer_Data_Request(ReadFormatCapacity_Data, READ_FORMAT_CAPACITY_DATA_LEN);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  SCSI_ReadCapacity10_Cmd — 返回容量 (8 字节, 最后 LBA + 块大小)
 * ═══════════════════════════════════════════════════════════════════════════ */
void SCSI_ReadCapacity10_Cmd(uint8_t lun)
{
    if (MAL_GetStatus(lun) != 0) {
        Set_Scsi_Sense_Data(CBW.bLUN, NOT_READY, MEDIUM_NOT_PRESENT);
        Set_CSW(CSW_CMD_FAILED, SEND_CSW_ENABLE);
        Bot_Abort(DIR_IN);
        return;
    }

    /* Last LBA = Block Count - 1 (大端) */
    ReadCapacity10_Data[0] = (uint8_t)((Mass_Block_Count[lun] - 1) >> 24);
    ReadCapacity10_Data[1] = (uint8_t)((Mass_Block_Count[lun] - 1) >> 16);
    ReadCapacity10_Data[2] = (uint8_t)((Mass_Block_Count[lun] - 1) >>  8);
    ReadCapacity10_Data[3] = (uint8_t)(Mass_Block_Count[lun] - 1);

    /* Block Length = 512 (大端) */
    ReadCapacity10_Data[4] = (uint8_t)(Mass_Block_Size[lun] >> 24);
    ReadCapacity10_Data[5] = (uint8_t)(Mass_Block_Size[lun] >> 16);
    ReadCapacity10_Data[6] = (uint8_t)(Mass_Block_Size[lun] >>  8);
    ReadCapacity10_Data[7] = (uint8_t)(Mass_Block_Size[lun]);

    Transfer_Data_Request(ReadCapacity10_Data, READ_CAPACITY10_DATA_LEN);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  SCSI_TestUnitReady_Cmd — 就绪检查
 * ═══════════════════════════════════════════════════════════════════════════ */
void SCSI_TestUnitReady_Cmd(uint8_t lun)
{
    if (MAL_GetStatus(lun) != 0) {
        Set_Scsi_Sense_Data(CBW.bLUN, NOT_READY, MEDIUM_NOT_PRESENT);
        Set_CSW(CSW_CMD_FAILED, SEND_CSW_ENABLE);
        Bot_Abort(DIR_IN);
        return;
    }

    Set_CSW(CSW_CMD_PASSED, SEND_CSW_ENABLE);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  SCSI_RequestSense_Cmd — 返回 Sense Data (18 字节)
 * ═══════════════════════════════════════════════════════════════════════════ */
void SCSI_RequestSense_Cmd(uint8_t lun)
{
    uint8_t Request_Sense_data_Length;

    if (CBW.CB[4] <= REQUEST_SENSE_DATA_LEN)
        Request_Sense_data_Length = CBW.CB[4];
    else
        Request_Sense_data_Length = REQUEST_SENSE_DATA_LEN;

    Transfer_Data_Request(Scsi_Sense_Data, Request_Sense_data_Length);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  SCSI_ModeSense6_Cmd / SCSI_ModeSense10_Cmd — 返回模式参数
 * ═══════════════════════════════════════════════════════════════════════════ */
void SCSI_ModeSense6_Cmd(uint8_t lun)
{
    Transfer_Data_Request(Mode_Sense6_data, MODE_SENSE6_DATA_LEN);
}

void SCSI_ModeSense10_Cmd(uint8_t lun)
{
    Transfer_Data_Request(Mode_Sense10_data, MODE_SENSE10_DATA_LEN);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  SCSI_Start_Stop_Unit_Cmd — 启停/弹出 (TR2-S2 暂返回 PASS)
 * ═══════════════════════════════════════════════════════════════════════════ */
void SCSI_Start_Stop_Unit_Cmd(uint8_t lun)
{
    Set_CSW(CSW_CMD_PASSED, SEND_CSW_ENABLE);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Set_Scsi_Sense_Data — 设置 Sense Key 和 ASC
 * ═══════════════════════════════════════════════════════════════════════════ */
void Set_Scsi_Sense_Data(uint8_t lun, uint8_t Sens_Key, uint8_t Asc)
{
    Scsi_Sense_Data[2]  = Sens_Key;
    Scsi_Sense_Data[12] = Asc;
}
