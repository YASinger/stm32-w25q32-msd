/**
  ******************************************************************************
  * @file    memory.c
  * @brief   缓冲调度层 — 64B 端点包与 512B 逻辑块的拆包/组包 (TR2-A4 / TR2-C1)
  *
  *          A4 阶段为骨架：MAL 调用用 #if 0 包裹。C1 恢复 Read_Memory 的
  *          MAL_Read（READ10 读路径）；Write_Memory 的 MAL_Write 留待 C2。
  *          Led_RW_* 删除（TR2 不引入 LED 读写指示）。
  *          缓冲调度逻辑（USB_SIL_Write 发送、Data_Buffer 拷贝、状态机
  *          流转、CSW.dDataResidue 递减）完整保留编译。
  *
  *          参考 ST 例程 memory.c。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "memory.h"
#include "usb_bot.h"      /* BULK_MAX_PACKET_SIZE / BOT_* / Set_CSW */
#include "mass_mal.h"     /* Mass_Block_Size / MAL_Read / MAL_Write */
#include "usb_lib.h"      /* USB_SIL_Write / SetEPTxCount / SetEPxStatus */

#include "usb_scsi.h"   /* B1: SCSI 宏/函数声明 (保持与完成态一致) */

/* Private variables ---------------------------------------------------------*/
__IO uint32_t Block_Read_count = 0;
__IO uint32_t Block_offset;
__IO uint32_t Counter = 0;
uint32_t Idx;
uint32_t Data_Buffer[BULK_MAX_PACKET_SIZE * 2];   /* 512 字节 */
uint8_t TransferState = TXFR_IDLE;

/* Extern variables ----------------------------------------------------------*/
extern uint8_t Bulk_Data_Buff[BULK_MAX_PACKET_SIZE];  /* BOT 层 OUT 接收缓冲 */
extern uint16_t Data_Len;
extern uint8_t Bot_State;
extern Bulk_Only_CBW CBW;
extern Bulk_Only_CSW CSW;
extern uint32_t Mass_Memory_Size[2];
extern uint32_t Mass_Block_Size[2];

/*******************************************************************************
* Function Name  : Read_Memory
* Description    : 读介质 → EP1 IN 拆包发送 (READ10 多包调度, C1 恢复完整功能)
*                  首次进入: 计算字节偏移/长度, 置 TXFR_ONGOING
*                  每包 64B: USB_SIL_Write 发送, EP1 IN 中断回调再次进入,
*                  直到 Length==0 置 BOT_DATA_IN_LAST
* Input          : lun - 逻辑单元号; Memory_Offset - LBA; Transfer_Length - 块数
* Output         : None.
* Return         : None.
*******************************************************************************/
void Read_Memory(uint8_t lun, uint32_t Memory_Offset, uint32_t Transfer_Length)
{
  static uint32_t Offset, Length;

  if (TransferState == TXFR_IDLE )
  {
    Offset = Memory_Offset * Mass_Block_Size[lun];
    Length = Transfer_Length * Mass_Block_Size[lun];
    TransferState = TXFR_ONGOING;
  }

  if (TransferState == TXFR_ONGOING )
  {
    if (!Block_Read_count)
    {
      MAL_Read(lun ,
               Offset ,
               Data_Buffer,
               Mass_Block_Size[lun]);

      USB_SIL_Write(EP1_IN, (uint8_t *)Data_Buffer, BULK_MAX_PACKET_SIZE);

      Block_Read_count = Mass_Block_Size[lun] - BULK_MAX_PACKET_SIZE;
      Block_offset = BULK_MAX_PACKET_SIZE;
    }
    else
    {
      USB_SIL_Write(EP1_IN, (uint8_t *)Data_Buffer + Block_offset, BULK_MAX_PACKET_SIZE);

      Block_Read_count -= BULK_MAX_PACKET_SIZE;
      Block_offset += BULK_MAX_PACKET_SIZE;
    }

    SetEPTxCount(ENDP1, BULK_MAX_PACKET_SIZE);
    SetEPTxStatus(ENDP1, EP_TX_VALID);
    Offset += BULK_MAX_PACKET_SIZE;
    Length -= BULK_MAX_PACKET_SIZE;

    CSW.dDataResidue -= BULK_MAX_PACKET_SIZE;
    /* Led_RW_ON();  TR2 不引入 LED 读写指示 */
  }

  if (Length == 0)
  {
    Block_Read_count = 0;
    Block_offset = 0;
    Offset = 0;
    Bot_State = BOT_DATA_IN_LAST;
    TransferState = TXFR_IDLE;
    /* Led_RW_OFF();  TR2 不引入 LED 读写指示 */
  }
}

/*******************************************************************************
* Function Name  : Write_Memory
* Description    : EP2 OUT 组包 → 写介质 (WRITE10 多包调度, C2 恢复完整功能)
*                  每包 64B 从 Bulk_Data_Buff 拷入 Data_Buffer, 凑满 512B
*                  触发介质写, 全部写完 (W_Length==0) 或上层要求发 CSW 时结束
* Input          : lun - 逻辑单元号; Memory_Offset - LBA; Transfer_Length - 块数
* Output         : None.
* Return         : None.
*******************************************************************************/
void Write_Memory (uint8_t lun, uint32_t Memory_Offset, uint32_t Transfer_Length)
{
  static uint32_t W_Offset, W_Length;

  uint32_t temp = Counter + BULK_MAX_PACKET_SIZE;

  if (TransferState == TXFR_IDLE )
  {
    W_Offset = Memory_Offset * Mass_Block_Size[lun];
    W_Length = Transfer_Length * Mass_Block_Size[lun];
    TransferState = TXFR_ONGOING;
  }

  if (TransferState == TXFR_ONGOING )
  {
    for (Idx = 0 ; Counter < temp; Counter++)
    {
      *((uint8_t *)Data_Buffer + Counter) = Bulk_Data_Buff[Idx++];
    }

    W_Offset += Data_Len;
    W_Length -= Data_Len;

    if (!(W_Length % Mass_Block_Size[lun]))
    {
      Counter = 0;
#if 0  /* === MAL 将 Data_Buffer 写入介质 (C2 恢复) === */
      MAL_Write(lun ,
                W_Offset - Mass_Block_Size[lun],
                Data_Buffer,
                Mass_Block_Size[lun]);
#endif
    }

    CSW.dDataResidue -= Data_Len;
    SetEPRxStatus(ENDP2, EP_RX_VALID); /* enable the next transaction */
    /* Led_RW_ON();  TR2 不引入 LED 读写指示 */
  }

  if ((W_Length == 0) || (Bot_State == BOT_CSW_Send))
  {
    Counter = 0;
    Set_CSW (CSW_CMD_PASSED, SEND_CSW_ENABLE);
    TransferState = TXFR_IDLE;
    /* Led_RW_OFF();  TR2 不引入 LED 读写指示 */
  }
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
