/**
  ******************************************************************************
  * @file    mass_mal.h
  * @brief   介质访问层 (Medium Access Layer) 接口
  ******************************************************************************
  */

#ifndef __MASS_MAL_H
#define __MASS_MAL_H

#include "stm32f10x.h"

#define MAL_OK      0
#define MAL_FAIL    1

/* ── 介质参数 (运行时由 MAL_Init 填充) ─────────────────────────────────────── */
extern uint32_t Mass_Memory_Size[2];
extern uint32_t Mass_Block_Size[2];
extern uint32_t Mass_Block_Count[2];

/* ── 接口 ──────────────────────────────────────────────────────────────────── */
uint16_t MAL_Init(uint8_t lun);
uint16_t MAL_Read(uint8_t lun, uint32_t Memory_Offset,
                  uint32_t *Readbuff, uint16_t Transfer_Length);
uint16_t MAL_Write(uint8_t lun, uint32_t Memory_Offset,
                   uint32_t *Writebuff, uint16_t Transfer_Length);
uint16_t MAL_GetStatus(uint8_t lun);

#endif /* __MASS_MAL_H */
