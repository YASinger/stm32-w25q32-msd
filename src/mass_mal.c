/**
  ******************************************************************************
  * @file    mass_mal.c
  * @brief   SRAM 介质访问层 — TR2-A1
  *
  *          用 8KB SRAM 数组模拟 U 盘存储。
  *          读写对称 (memcpy)，断电丢失。
  *          TR3 将替换为 W25Q32 Flash 实现。
  ******************************************************************************
  */

#include "mass_mal.h"
#include <string.h>

/* ── SRAM 磁盘 (8KB = 16 块 × 512B) ──────────────────────────────────────── */
#define SRAM_DISK_SIZE   8192

static uint8_t sram_disk[SRAM_DISK_SIZE];

/* ── 全局变量 (SCSI 层读取容量信息) ──────────────────────────────────────── */
uint32_t Mass_Memory_Size[2];
uint32_t Mass_Block_Size[2];
uint32_t Mass_Block_Count[2];

/*******************************************************************************
* Function Name  : MAL_Init
* Description    : 初始化介质 (SRAM 无需初始化)
* Input          : lun - 逻辑单元号 (本项目仅 0)
* Return         : MAL_OK / MAL_FAIL
*******************************************************************************/
uint16_t MAL_Init(uint8_t lun)
{
    if (lun != 0) return MAL_FAIL;

    /* SRAM 无需硬件初始化; 填充 0xFF 模拟 Flash 擦除态 (C1) */
    memset(sram_disk, 0xFF, SRAM_DISK_SIZE);
    return MAL_OK;
}

/*******************************************************************************
* Function Name  : MAL_GetStatus
* Description    : 查询介质状态, 填充容量信息
* Input          : lun - 逻辑单元号
* Return         : MAL_OK / MAL_FAIL
* Note           : SCSI_ReadCapacity10 / SCSI_TestUnitReady 通过此函数获取容量
*******************************************************************************/
uint16_t MAL_GetStatus(uint8_t lun)
{
    if (lun != 0) return MAL_FAIL;

    Mass_Memory_Size[0]  = SRAM_DISK_SIZE;         /* 8192 字节 */
    Mass_Block_Size[0]   = 512;                    /* 512 字节/块 */
    Mass_Block_Count[0]  = SRAM_DISK_SIZE / 512;   /* 16 块 */

    return MAL_OK;
}

/*******************************************************************************
* Function Name  : MAL_Read
* Description    : 从 SRAM 磁盘读取数据
* Input          : lun             - 逻辑单元号
*                  Memory_Offset   - 字节偏移
*                  Readbuff        - 输出缓冲区
*                  Transfer_Length - 读取字节数
* Return         : MAL_OK / MAL_FAIL
*******************************************************************************/
uint16_t MAL_Read(uint8_t lun, uint32_t Memory_Offset, uint32_t *Readbuff, uint16_t Transfer_Length)
{
    if (lun != 0) return MAL_FAIL;
    if (Memory_Offset + Transfer_Length > SRAM_DISK_SIZE) return MAL_FAIL;

    memcpy((uint8_t *)Readbuff, &sram_disk[Memory_Offset], Transfer_Length);
    return MAL_OK;
}

/*******************************************************************************
* Function Name  : MAL_Write
* Description    : 向 SRAM 磁盘写入数据
* Input          : lun             - 逻辑单元号
*                  Memory_Offset   - 字节偏移
*                  Writebuff       - 输入缓冲区
*                  Transfer_Length - 写入字节数
* Return         : MAL_OK / MAL_FAIL
* Note           : SRAM 读写对称, 直接 memcpy 覆盖, 无需擦除
*                  (Flash 需先擦后写, TR3 实现)
*******************************************************************************/
uint16_t MAL_Write(uint8_t lun, uint32_t Memory_Offset, uint32_t *Writebuff, uint16_t Transfer_Length)
{
    if (lun != 0) return MAL_FAIL;
    if (Memory_Offset + Transfer_Length > SRAM_DISK_SIZE) return MAL_FAIL;

    memcpy(&sram_disk[Memory_Offset], (uint8_t *)Writebuff, Transfer_Length);
    return MAL_OK;
}
