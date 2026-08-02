/**
  ******************************************************************************
  * @file    usb_scsi.h
  * @brief   SCSI 命令宏 + Sense Key/ASC 宏 + 函数声明 (TR2-B1 / TR2-C1 / TR2-C2)
  *
  *          参考 ST 例程 usb_scsi.h，裁剪：
  *          - 去掉 hw_config.h 依赖，自包含 stm32f10x.h + usb_type.h (bool)
  *          - 数据长度宏与静态数据 extern 复用 scsi_data.h (A2)，不重复定义
  *          - 去掉 Standard_Inquiry_Data2 extern（单 LUN）
  *          - SCSI_READ10/SCSI_WRITE10 用 #ifndef 守卫（与 usb_bot.h 占位共存）
  *          - C1 声明 SCSI_Read10_Cmd / SCSI_Address_Management (bool)
  *          - C2 声明 SCSI_Write10_Cmd
  ******************************************************************************
  */

#ifndef __USB_SCSI_H
#define __USB_SCSI_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x.h"
#include "usb_type.h"     /* C1: bool 类型 (SCSI_Address_Management 返回类型) */
#include "scsi_data.h"    /* 数据长度宏 + 静态数据 extern (A2 定义) */

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/

/* ==== SCSI Commands ==== */
#define SCSI_FORMAT_UNIT               0x04
#define SCSI_INQUIRY                   0x12
#define SCSI_MODE_SELECT6              0x15
#define SCSI_MODE_SELECT10             0x55
#define SCSI_MODE_SENSE6               0x1A
#define SCSI_MODE_SENSE10              0x5A
#define SCSI_ALLOW_MEDIUM_REMOVAL      0x1E
#define SCSI_READ6                     0x08
#ifndef SCSI_READ10                    /* usb_bot.h 已有 #ifndef 占位 */
#define SCSI_READ10                    0x28
#endif
#define SCSI_READ12                    0xA8
#define SCSI_READ16                    0x88
#define SCSI_READ_CAPACITY10           0x25
#define SCSI_READ_CAPACITY16           0x9E
#define SCSI_REQUEST_SENSE             0x03
#define SCSI_START_STOP_UNIT           0x1B
#define SCSI_TEST_UNIT_READY           0x00
#define SCSI_WRITE6                    0x0A
#ifndef SCSI_WRITE10
#define SCSI_WRITE10                   0x2A
#endif
#define SCSI_WRITE12                   0xAA
#define SCSI_WRITE16                   0x8A
#define SCSI_VERIFY10                  0x2F
#define SCSI_VERIFY12                  0xAF
#define SCSI_VERIFY16                  0x8F
#define SCSI_SEND_DIAGNOSTIC           0x1D
#define SCSI_READ_FORMAT_CAPACITIES    0x23

/* ==== Sense Key ==== */
#define NO_SENSE                       0
#define RECOVERED_ERROR                1
#define NOT_READY                      2
#define MEDIUM_ERROR                   3
#define HARDWARE_ERROR                 4
#define ILLEGAL_REQUEST                5
#define UNIT_ATTENTION                 6
#define DATA_PROTECT                   7
#define BLANK_CHECK                    8
#define VENDOR_SPECIFIC                9
#define COPY_ABORTED                   10
#define ABORTED_COMMAND                11
#define VOLUME_OVERFLOW                13
#define MISCOMPARE                     14

/* ==== ASC (Additional Sense Code) ==== */
#define INVALID_COMMAND                0x20
#define INVALID_FIELED_IN_COMMAND      0x24
#define PARAMETER_LIST_LENGTH_ERROR    0x1A
#define INVALID_FIELD_IN_PARAMETER_LIST 0x26
#define ADDRESS_OUT_OF_RANGE           0x21
#define MEDIUM_NOT_PRESENT             0x3A
#define MEDIUM_HAVE_CHANGED            0x28

#define BLKVFY                         0x04   /* VERIFY10 用 (C3), 先定义 */

/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */

/* B1 实现的查询命令 */
void SCSI_Inquiry_Cmd(uint8_t lun);
void SCSI_ReadFormatCapacity_Cmd(uint8_t lun);
void SCSI_ReadCapacity10_Cmd(uint8_t lun);
void SCSI_RequestSense_Cmd(uint8_t lun);
void SCSI_Start_Stop_Unit_Cmd(uint8_t lun);
void SCSI_ModeSense6_Cmd(uint8_t lun);
void SCSI_ModeSense10_Cmd(uint8_t lun);
void SCSI_TestUnitReady_Cmd(uint8_t lun);
void Set_Scsi_Sense_Data(uint8_t lun, uint8_t Sens_Key, uint8_t Asc);

/* 不支持命令的统一处理 */
void SCSI_Invalid_Cmd(uint8_t lun);
void SCSI_Valid_Cmd(uint8_t lun);
#define SCSI_Prevent_Removal_Cmd        SCSI_Valid_Cmd

/* Invalid (Unsupported) commands → 宏别名 */
#define SCSI_READ_CAPACITY16_Cmd        SCSI_Invalid_Cmd
#define SCSI_Write6_Cmd                 SCSI_Invalid_Cmd
#define SCSI_Write12_Cmd                SCSI_Invalid_Cmd
#define SCSI_Write16_Cmd                SCSI_Invalid_Cmd
#define SCSI_Read6_Cmd                  SCSI_Invalid_Cmd
#define SCSI_Read12_Cmd                 SCSI_Invalid_Cmd
#define SCSI_Read16_Cmd                 SCSI_Invalid_Cmd
#define SCSI_Send_Diagnostic_Cmd        SCSI_Invalid_Cmd
#define SCSI_Mode_Select6_Cmd           SCSI_Invalid_Cmd
#define SCSI_Mode_Select10_Cmd          SCSI_Invalid_Cmd
#define SCSI_Verify12_Cmd               SCSI_Invalid_Cmd
#define SCSI_Verify16_Cmd               SCSI_Invalid_Cmd

/* C1: READ10 数据命令 */
void SCSI_Read10_Cmd(uint8_t lun, uint32_t LBA, uint32_t BlockNbr);
bool SCSI_Address_Management(uint8_t lun, uint8_t Cmd, uint32_t LBA, uint32_t BlockNbr);

/* C2: WRITE10 数据命令 */
void SCSI_Write10_Cmd(uint8_t lun, uint32_t LBA, uint32_t BlockNbr);

/* C3 再声明: SCSI_Verify10_Cmd / SCSI_Format_Cmd */

#endif /* __USB_SCSI_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
