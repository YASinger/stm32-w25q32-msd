/**
  ******************************************************************************
  * @file    scsi_data.c
  * @brief   SCSI 响应数据表 (静态常量, 容量相关字段运行时填充)
  ******************************************************************************
  */

#include "usb_scsi.h"

/* ── VPD Page00 响应 (5 字节, Evpd=1 时返回) ──────────────────────────────── */
uint8_t Page00_Inquiry_Data[] = {
    0x00,   /* Peripheral Qualifier + Device Type: Direct Access */
    0x00,
    0x00,
    0x00,
    0x00    /* Supported Pages: 无 (空列表) */
};

/* ── 标准 INQUIRY 响应 (36 字节) ──────────────────────────────────────────── */
uint8_t Standard_Inquiry_Data[] = {
    /* 0 */ 0x00,          /* Peripheral Qualifier=0, Device Type=0 (Direct Access) */
    /* 1 */ 0x80,          /* RMB=1: 可移除介质 */
    /* 2 */ 0x02,          /* Version: 不声称符合标准 */
    /* 3 */ 0x02,          /* Response Format: SCSI-2 */
    /* 4 */ 36 - 4,        /* Additional Length = 32 */
    /* 5 */ 0x00,          /* SCCS 等 */
    /* 6 */ 0x00,
    /* 7 */ 0x00,
    /* 8-15  Vendor: "STM     " (8 字节) */
    'S', 'T', 'M', ' ', ' ', ' ', ' ', ' ',
    /* 16-31 Product: "W25Q32 FlashDisk " (16 字节) */
    'W', '2', '5', 'Q', '3', '2', ' ', 'F',
    'l', 'a', 's', 'h', 'D', 'i', 's', 'k',
    /* 32-35 Revision: "1.00" (4 字节) */
    '1', '.', '0', '0'
};

/* ── ModeSense6 响应 (4 字节) ──────────────────────────────────────────────── */
uint8_t Mode_Sense6_data[] = {
    0x03,   /* Mode Data Length = 3 */
    0x00,   /* Medium Type = 0 */
    0x00,   /* Device-Specific Parameter */
    0x00    /* Block Descriptor Length = 0 */
};

/* ── ModeSense10 响应 (8 字节) ─────────────────────────────────────────────── */
uint8_t Mode_Sense10_data[] = {
    0x00,   /* Mode Data Length (高字节) */
    0x06,   /* Mode Data Length (低字节) = 6 */
    0x00,   /* Medium Type = 0 */
    0x00,   /* Device-Specific Parameter */
    0x00,   /* Reserved */
    0x00,   /* Reserved */
    0x00,   /* Block Descriptor Length (高字节) */
    0x00    /* Block Descriptor Length (低字节) = 0 */
};

/* ── RequestSense 响应 (18 字节) ───────────────────────────────────────────── */
uint8_t Scsi_Sense_Data[] = {
    0x70,       /* Response Code: Current Error */
    0x00,       /* Segment Number */
    NO_SENSE,   /* Sense Key (运行时由 Set_Scsi_Sense_Data 修改) */
    0x00, 0x00, 0x00, 0x00,   /* Information */
    0x0A,       /* Additional Sense Length = 10 */
    0x00, 0x00, 0x00, 0x00,   /* Command-Specific Information */
    NO_SENSE,   /* ASC (运行时由 Set_Scsi_Sense_Data 修改) */
    0x00,       /* ASCQ */
    0x00,       /* FRUC */
    0x00, 0x00  /* Sense Key Specific */
};

/* ── ReadCapacity10 响应 (8 字节, 运行时填充) ─────────────────────────────── */
uint8_t ReadCapacity10_Data[] = {
    0, 0, 0, 0,   /* Last LBA (运行时填充) */
    0, 0, 0, 0    /* Block Length (运行时填充) */
};

/* ── ReadFormatCapacity 响应 (12 字节, 运行时填充) ────────────────────────── */
uint8_t ReadFormatCapacity_Data[] = {
    0x00, 0x00, 0x00, 0x08,   /* Header: Capacity List Length = 8 */
    0, 0, 0, 0,               /* Block Count (运行时填充) */
    0x02,                     /* Descriptor Code: Formatted Media */
    0, 0, 0                   /* Block Length (运行时填充) */
};
