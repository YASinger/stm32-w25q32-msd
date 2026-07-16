/**
  ******************************************************************************
  * @file    usb_scsi.h
  * @brief   SCSI 命令码、Sense Key、数据长度、函数声明
  ******************************************************************************
  */

#ifndef __USB_SCSI_H
#define __USB_SCSI_H

#include "stm32f10x.h"

/* ── SCSI 命令码 ───────────────────────────────────────────────────────────── */
#define SCSI_TEST_UNIT_READY        0x00
#define SCSI_REQUEST_SENSE          0x03
#define SCSI_FORMAT_UNIT            0x04
#define SCSI_INQUIRY                0x12
#define SCSI_MODE_SENSE6            0x1A
#define SCSI_MODE_SENSE10           0x5A
#define SCSI_START_STOP_UNIT        0x1B
#define SCSI_ALLOW_MEDIUM_REMOVAL   0x1E
#define SCSI_READ_FORMAT_CAPACITIES 0x23
#define SCSI_READ_CAPACITY10        0x25
#define SCSI_READ10                 0x28
#define SCSI_WRITE10                0x2A
#define SCSI_VERIFY10               0x2F

/* ── Sense Key ──────────────────────────────────────────────────────────────── */
#define NO_SENSE                    0
#define RECOVERED_ERROR             1
#define NOT_READY                   2
#define MEDIUM_ERROR                3
#define HARDWARE_ERROR              4
#define ILLEGAL_REQUEST             5
#define UNIT_ATTENTION              6
#define DATA_PROTECT                7
#define BLANK_CHECK                 8

/* ── Additional Sense Code ─────────────────────────────────────────────────── */
#define INVALID_COMMAND             0x20
#define ADDRESS_OUT_OF_RANGE        0x21
#define INVALID_FIELED_IN_COMMAND   0x24
#define PARAMETER_LIST_LENGTH_ERROR 0x1A
#define MEDIUM_NOT_PRESENT          0x3A
#define MEDIUM_HAVE_CHANGED         0x28

/* ── 响应数据长度 ───────────────────────────────────────────────────────────── */
#define READ_FORMAT_CAPACITY_DATA_LEN   0x0C    /* 12 */
#define READ_CAPACITY10_DATA_LEN        0x08    /* 8 */
#define MODE_SENSE6_DATA_LEN            0x04    /* 4 */
#define MODE_SENSE10_DATA_LEN           0x08    /* 8 */
#define REQUEST_SENSE_DATA_LEN          0x12    /* 18 */
#define STANDARD_INQUIRY_DATA_LEN       0x24    /* 36 */
#define BLKVFY                          0x04

/* ── SCSI 响应数据 extern 声明 ─────────────────────────────────────────────── */
extern uint8_t Page00_Inquiry_Data[];
extern uint8_t Standard_Inquiry_Data[];
extern uint8_t Mode_Sense6_data[];
extern uint8_t Mode_Sense10_data[];
extern uint8_t Scsi_Sense_Data[];
extern uint8_t ReadCapacity10_Data[];
extern uint8_t ReadFormatCapacity_Data[];

/* ── 函数声明 ──────────────────────────────────────────────────────────────── */
void SCSI_Inquiry_Cmd(uint8_t lun);
void SCSI_ReadFormatCapacity_Cmd(uint8_t lun);
void SCSI_ReadCapacity10_Cmd(uint8_t lun);
void SCSI_RequestSense_Cmd(uint8_t lun);
void SCSI_Start_Stop_Unit_Cmd(uint8_t lun);
void SCSI_ModeSense6_Cmd(uint8_t lun);
void SCSI_ModeSense10_Cmd(uint8_t lun);
void Set_Scsi_Sense_Data(uint8_t lun, uint8_t Sens_Key, uint8_t Asc);
void SCSI_TestUnitReady_Cmd(uint8_t lun);

#endif /* __USB_SCSI_H */
