#ifndef __SCSI_DATA_H
#define __SCSI_DATA_H

#include "stm32f10x.h"

/* 数据长度宏 (SCSI 命令处理函数引用) */
#define READ_FORMAT_CAPACITY_DATA_LEN   0x0C    /* 12 */
#define READ_CAPACITY10_DATA_LEN        0x08    /* 8  */
#define MODE_SENSE10_DATA_LEN           0x08    /* 8  */
#define MODE_SENSE6_DATA_LEN            0x04    /* 4  */
#define REQUEST_SENSE_DATA_LEN          0x12    /* 18 */
#define STANDARD_INQUIRY_DATA_LEN       0x24    /* 36 */

/* 静态数据数组 */
extern uint8_t Page00_Inquiry_Data[];
extern uint8_t Standard_Inquiry_Data[];
extern uint8_t Mode_Sense6_data[];
extern uint8_t Mode_Sense10_data[];
extern uint8_t Scsi_Sense_Data[];
extern uint8_t ReadCapacity10_Data[];
extern uint8_t ReadFormatCapacity_Data[];

#endif /* __SCSI_DATA_H */
