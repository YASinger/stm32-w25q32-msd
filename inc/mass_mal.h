#ifndef __MASS_MAL_H
#define __MASS_MAL_H

#include "stm32f10x.h"

#define MAL_OK   0
#define MAL_FAIL 1
#define MAX_LUN  0               /* 仅 LUN 0 */

/* 全局变量 (SCSI 层通过这些变量获取介质容量信息) */
extern uint32_t Mass_Memory_Size[2];
extern uint32_t Mass_Block_Size[2];
extern uint32_t Mass_Block_Count[2];

uint16_t MAL_Init(uint8_t lun);
uint16_t MAL_GetStatus(uint8_t lun);
uint16_t MAL_Read(uint8_t lun, uint32_t Memory_Offset, uint32_t *Readbuff, uint16_t Transfer_Length);
uint16_t MAL_Write(uint8_t lun, uint32_t Memory_Offset, uint32_t *Writebuff, uint16_t Transfer_Length);

#endif /* __MASS_MAL_H */
