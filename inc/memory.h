/**
  ******************************************************************************
  * @file    memory.h
  * @brief   缓冲调度层 — Read_Memory/Write_Memory 接口 (TR2-A4)
  *
  *          BOT 四层架构中的"缓冲调度层"：负责 64B 端点包与 512B 逻辑块
  *          之间的拆包/组包。A4 阶段为骨架（无调用方，MAL 调用在 memory.c
  *          中用 #if 0 包裹），C1/C2 与 SCSI READ10/WRITE10 一起恢复。
  *
  *          参考 ST 例程 memory.h；与完成态的差异：
  *          - 不包含 hw_config.h（无依赖），改为 stm32f10x.h 自包含类型
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MEMORY_H
#define __MEMORY_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x.h"   /* uint8_t/uint32_t 类型 */

/* Exported constants --------------------------------------------------------*/

/* 传输状态：A4 骨架用 TXFR_ONGOING 区分多包续传的首次进入 */
#define TXFR_IDLE     0
#define TXFR_ONGOING  1

/* Exported functions ------------------------------------------------------- */

/* Read_Memory: 将介质数据拆包发送到 EP1 IN (READ10 多包调度, C1 恢复完整功能)
 * Write_Memory: 将 EP2 OUT 数据组包写入介质 (WRITE10 多包调度, C2 恢复完整功能)
 * lun - 逻辑单元号; Memory_Offset - LBA; Transfer_Length - 块数 */
void Read_Memory (uint8_t lun, uint32_t Memory_Offset, uint32_t Transfer_Length);
void Write_Memory (uint8_t lun, uint32_t Memory_Offset, uint32_t Transfer_Length);

#endif /* __MEMORY_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
