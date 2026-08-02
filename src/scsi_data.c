/**
  ******************************************************************************
  * @file    scsi_data.c
  * @brief   SCSI 静态响应数据 — TR2-A2
  *
  *          主机发 INQUIRY/REQUEST_SENSE/MODE_SENSE 等查询命令时
  *          返回的预填充数据。ReadCapacity 的容量字段运行时填充。
  ******************************************************************************
  */

#include "scsi_data.h"

/* ── Vital Product Data Page 0x00 ────────────────────────────────────────── */
uint8_t Page00_Inquiry_Data[] = {
    0x00,   /* PERIPHERAL QUALIFIER & PERIPHERAL DEVICE TYPE */
    0x00,
    0x00,
    0x00,
    0x00    /* Supported Pages: 00 */
};

/* ── Standard Inquiry Data (36 字节) ─────────────────────────────────────── */
uint8_t Standard_Inquiry_Data[] = {
    0x00,       /* Direct Access Device */
    0x80,       /* RMB = 1: Removable Medium */
    0x02,       /* Version: No conformance claim to standard */
    0x02,
    36 - 4,     /* Additional Length */
    0x00,       /* SCCS */
    0x00,
    0x00,
    /* Vendor Identification (8 字节) */
    'S', 'T', 'M', ' ', ' ', ' ', ' ', ' ',
    /* Product Identification (16 字节) */
    'S', 'T', 'M', '3', '2', ' ', 'S', 'R', 'A', 'M', ' ', 'D', 'i', 's', 'k', ' ',
    /* Product Revision Level (4 字节) */
    '1', '.', '0', ' '
};

/* ── Mode Sense6 Data (4 字节) ──────────────────────────────────────────── */
uint8_t Mode_Sense6_data[] = {
    0x03,
    0x00,
    0x00,
    0x00,
};

/* ── Mode Sense10 Data (8 字节) ─────────────────────────────────────────── */
uint8_t Mode_Sense10_data[] = {
    0x00,
    0x06,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00
};

/* ── Sense Data (18 字节, 运行时由 Set_Scsi_Sense_Data 修改) ─────────────── */
uint8_t Scsi_Sense_Data[] = {
    0x70,   /* Response Code: Fixed format */
    0x00,   /* Segment Number */
    0x00,   /* Sense Key: NO_SENSE (运行时修改) */
    0x00,
    0x00,
    0x00,
    0x00,   /* Information */
    0x0A,   /* Additional Sense Length: 10 */
    0x00,
    0x00,
    0x00,
    0x00,   /* Cmd Information */
    0x00,   /* ASC (运行时修改) */
    0x00,   /* ASCQ */
    0x00,   /* FRUC */
    0x00,   /* TBD */
    0x00,
    0x00    /* Sense Key Specific */
};

/* ── Read Capacity10 Data (8 字节, 运行时填充容量) ──────────────────────── */
uint8_t ReadCapacity10_Data[] = {
    /* Last Logical Block (4 字节, 运行时填 Mass_Block_Count-1) */
    0, 0, 0, 0,
    /* Block Length (4 字节, 运行时填 Mass_Block_Size=512=0x200) */
    0, 0, 0, 0
};

/* ── Read Format Capacity Data (12 字节, 运行时填充容量) ────────────────── */
uint8_t ReadFormatCapacity_Data[] = {
    0x00,
    0x00,
    0x00,
    0x08,   /* Capacity List Length: 8 */
    /* Block Count (4 字节, 运行时填) */
    0, 0, 0, 0,
    /* Block Length (4 字节: 高位含 Descriptor Type 2bit + Block Length 30bit)
       运行时填 [9..11] = 512, [8] 必须为 0 (原 0x02 会被解析成 33.5MB 错误块长) */
    0x00,
    0, 0, 0
};
