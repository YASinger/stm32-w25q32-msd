/**
  ******************************************************************************
  * @file    mass_mal.c
  * @brief   SRAM 介质访问层 — 8KB 虚拟磁盘存储
  ******************************************************************************
  */

#include "mass_mal.h"
#include <string.h>

/* ── 8KB SRAM 缓冲区 — 用作虚拟磁盘的存储介质 ─────────────────────────────── */
#define MAL_RAM_SIZE    8192
static uint8_t SRAM_Buffer[MAL_RAM_SIZE];

/* ── 介质参数 ─────────────────────────────────────────────────────────────── */
uint32_t Mass_Memory_Size[2];
uint32_t Mass_Block_Size[2];
uint32_t Mass_Block_Count[2];

/* ═══════════════════════════════════════════════════════════════════════════
 *  MAL_Init — 初始化介质 (清零 SRAM, 设置容量参数)
 * ═══════════════════════════════════════════════════════════════════════════ */
uint16_t MAL_Init(uint8_t lun)
{
    if (lun > 0) return MAL_FAIL;

    memset(SRAM_Buffer, 0, MAL_RAM_SIZE);

    Mass_Block_Size[0]  = 512;       /* 每扇区 512 字节 */
    Mass_Block_Count[0] = 16;        /* 16 个扇区 */
    Mass_Memory_Size[0] = 8192;      /* 总容量 8KB */

    return MAL_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  MAL_Read — 从 SRAM 读取数据
 * ═══════════════════════════════════════════════════════════════════════════ */
uint16_t MAL_Read(uint8_t lun, uint32_t Memory_Offset,
                  uint32_t *Readbuff, uint16_t Transfer_Length)
{
    if (lun > 0) return MAL_FAIL;
    if (Memory_Offset + Transfer_Length > MAL_RAM_SIZE) return MAL_FAIL;

    memcpy(Readbuff, &SRAM_Buffer[Memory_Offset], Transfer_Length);
    return MAL_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  MAL_Write — 向 SRAM 写入数据
 * ═══════════════════════════════════════════════════════════════════════════ */
uint16_t MAL_Write(uint8_t lun, uint32_t Memory_Offset,
                   uint32_t *Writebuff, uint16_t Transfer_Length)
{
    if (lun > 0) return MAL_FAIL;
    if (Memory_Offset + Transfer_Length > MAL_RAM_SIZE) return MAL_FAIL;

    memcpy(&SRAM_Buffer[Memory_Offset], Writebuff, Transfer_Length);
    return MAL_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  MAL_GetStatus — 获取介质状态 (SRAM 始终就绪)
 * ═══════════════════════════════════════════════════════════════════════════ */
uint16_t MAL_GetStatus(uint8_t lun)
{
    if (lun > 0) return MAL_FAIL;
    return MAL_OK;   /* SRAM 始终就绪 */
}
