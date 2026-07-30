/**
  ******************************************************************************
  * @file    usb_bot.h
  * @brief   BOT 状态机 — CBW/CSW 结构体与状态机接口 (TR2-A3)
  *
  *          参考 ST 例程 usb_bot.h，在原版基础上补入 BULK_MAX_PACKET_SIZE
  *          及 SCSI_READ10/SCSI_WRITE10 宏的占位（B1 创建 usb_scsi.h 时
  *          会在该头文件中重新定义；此处用 #ifndef 守卫避免重复定义）。
  ******************************************************************************
  */

#ifndef __USB_BOT_H
#define __USB_BOT_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x.h"

/* Exported types ------------------------------------------------------------*/

/* Bulk-only Command Block Wrapper (31 字节) */
typedef struct _Bulk_Only_CBW
{
  uint32_t dSignature;
  uint32_t dTag;
  uint32_t dDataLength;
  uint8_t  bmFlags;
  uint8_t  bLUN;
  uint8_t  bCBLength;
  uint8_t  CB[16];
}
Bulk_Only_CBW;

/* Bulk-only Command Status Wrapper (13 字节) */
typedef struct _Bulk_Only_CSW
{
  uint32_t dSignature;
  uint32_t dTag;
  uint32_t dDataResidue;
  uint8_t  bStatus;
}
Bulk_Only_CSW;

/* Exported constants --------------------------------------------------------*/

/* Bulk-Only 包大小 */
#define BULK_MAX_PACKET_SIZE             0x40    /* 64 字节 */

/*****************************************************************************/
/*********************** Bulk-Only Transfer State machine ********************/
/*****************************************************************************/
#define BOT_IDLE                      0       /* Idle state */
#define BOT_DATA_OUT                  1       /* Data Out state */
#define BOT_DATA_IN                   2       /* Data In state */
#define BOT_DATA_IN_LAST              3       /* Last Data In Last */
#define BOT_CSW_Send                  4       /* Command Status Wrapper */
#define BOT_ERROR                     5       /* error state */

#define BOT_CBW_SIGNATURE             0x43425355
#define BOT_CSW_SIGNATURE             0x53425355
#define BOT_CBW_PACKET_LENGTH         31

#define CSW_DATA_LENGTH               0x000D

/* CSW Status Definitions */
#define CSW_CMD_PASSED                0x00
#define CSW_CMD_FAILED                0x01
#define CSW_PHASE_ERROR               0x02

#define SEND_CSW_DISABLE              0
#define SEND_CSW_ENABLE               1

#define DIR_IN                        0
#define DIR_OUT                       1
#define BOTH_DIR                      2

/*
 * SCSI 命令操作码占位 — CBW_Decode() 解析 LBA/BlkLen 时引用。
 * B1 创建 usb_scsi.h 时会以 #ifndef 守卫方式重定义这两个宏，
 * 避免重复定义冲突。
 */
#ifndef SCSI_READ10
  #define SCSI_READ10                 0x28
#endif
#ifndef SCSI_WRITE10
  #define SCSI_WRITE10                0x2A
#endif

/* Exported functions ------------------------------------------------------- */
void Mass_Storage_In (void);
void Mass_Storage_Out (void);
void CBW_Decode(void);
void Transfer_Data_Request(uint8_t* Data_Pointer, uint16_t Data_Len);
void Set_CSW (uint8_t CSW_Status, uint8_t Send_Permission);
void Bot_Abort(uint8_t Direction);

#endif /* __USB_BOT_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
