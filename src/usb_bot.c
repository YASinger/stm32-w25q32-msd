/**
  ******************************************************************************
  * @file    usb_bot.c
  * @brief   BOT 状态机 + SCSI 命令分发 (TR2-A3 / TR2-B1)
  *
  *          TR2-A3 建立骨架：CBW 解码/CSW 返回/状态机流转，SCSI 命令
  *          分发用 #if 0 包裹（全部返回 CSW_CMD_FAILED）。
  *          TR2-B1 接入 usb_scsi.c：恢复 21 个查询/不支持命令 case 与
  *          4 处 Set_Scsi_Sense_Data() 调用。READ10/WRITE10/VERIFY10/
  *          FORMAT_UNIT 仍 #if 0（C1/C2/C3 恢复）。
  *            - Mass_Storage_In() 的 BOT_DATA_IN 分支用 #if 0 包裹 (C1)
  *            - Mass_Storage_Out() 的 BOT_DATA_OUT 分支用 #if 0 包裹 (C2)
  *            - Max_Lun 变量替换为 mass_mal.h 的 MAX_LUN 宏
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "usb_lib.h"
#include "usb_bot.h"
#include "mass_mal.h"
#include "usb_scsi.h"    /* B1: SCSI 命令宏 + 查询命令函数声明 */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
uint8_t  Bot_State;
uint8_t  Bulk_Data_Buff[BULK_MAX_PACKET_SIZE];   /* 数据缓冲区 */
uint16_t Data_Len;
Bulk_Only_CBW CBW;
Bulk_Only_CSW CSW;
uint32_t SCSI_LBA, SCSI_BlkLen;

/* Extern variables ----------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/*******************************************************************************
* Function Name  : Mass_Storage_In
* Description    : EP1 IN 传输完成回调，按 Bot_State 推进状态机。
*                  - BOT_CSW_Send / BOT_ERROR: CSW 已发完，回到 IDLE 重新接收 CBW
*                  - BOT_DATA_IN_LAST: 数据已发完，进入 CSW 阶段
*                  - BOT_DATA_IN: READ10 多包续传 (B1/C1 恢复)
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
void Mass_Storage_In(void)
{
  switch (Bot_State)
  {
    case BOT_CSW_Send:
    case BOT_ERROR:
      Bot_State = BOT_IDLE;
      SetEPRxStatus(ENDP2, EP_RX_VALID);   /* 重新使能 EP2 接收下一条 CBW */
      break;

#if 0  /* === BOT_DATA_IN 分支依赖 SCSI_Read10_Cmd (B1/C1 恢复) === */
    case BOT_DATA_IN:
      switch (CBW.CB[0])
      {
        case SCSI_READ10:
          SCSI_Read10_Cmd(CBW.bLUN, SCSI_LBA, SCSI_BlkLen);
          break;
      }
      break;
#endif

    case BOT_DATA_IN_LAST:
      Set_CSW(CSW_CMD_PASSED, SEND_CSW_ENABLE);
      SetEPRxStatus(ENDP2, EP_RX_VALID);
      break;

    default:
      break;
  }
}

/*******************************************************************************
* Function Name  : Mass_Storage_Out
* Description    : EP2 OUT 接收完成回调，按 Bot_State 推进状态机。
*                  - BOT_IDLE: 读出 CBW，进入 CBW_Decode
*                  - BOT_DATA_OUT: WRITE10 数据续传 (B1/C2 恢复)
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
void Mass_Storage_Out(void)
{
  Data_Len = USB_SIL_Read(EP2_OUT, Bulk_Data_Buff);

  switch (Bot_State)
  {
    case BOT_IDLE:
      CBW_Decode();
      break;

#if 0  /* === BOT_DATA_OUT 分支依赖 SCSI_Write10_Cmd (B1/C2 恢复) === */
    case BOT_DATA_OUT:
      if (CBW.CB[0] == SCSI_WRITE10)
      {
        SCSI_Write10_Cmd(CBW.bLUN, SCSI_LBA, SCSI_BlkLen);
        break;
      }
      Bot_Abort(DIR_OUT);
      Set_CSW(CSW_PHASE_ERROR, SEND_CSW_DISABLE);
      break;
#endif

    default:
      Bot_Abort(BOTH_DIR);
      Set_CSW(CSW_PHASE_ERROR, SEND_CSW_DISABLE);
      break;
  }
}

/*******************************************************************************
* Function Name  : CBW_Decode
* Description    : 解析 Bulk_Data_Buff 中的 CBW 并按 SCSI 操作码分发。
*                  A3 阶段所有 SCSI 命令处理用 #if 0 包裹，只保留 default
*                  分支返回 CSW_CMD_FAILED。
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
void CBW_Decode(void)
{
  uint32_t Counter;

  /* 将 Bulk_Data_Buff 拷贝到 CBW 结构体 */
  for (Counter = 0; Counter < Data_Len; Counter++)
  {
    *((uint8_t *)&CBW + Counter) = Bulk_Data_Buff[Counter];
  }

  /* CSW 中先填好 dTag / dDataResidue，后续 Set_CSW 直接复用 */
  CSW.dTag = CBW.dTag;
  CSW.dDataResidue = CBW.dDataLength;

  /* 长度不对，直接失败 */
  if (Data_Len != BOT_CBW_PACKET_LENGTH)
  {
    Bot_Abort(BOTH_DIR);
    /* 清签名以禁用 clear feature，直到收到 Mass Storage Reset */
    CBW.dSignature = 0;
    Set_Scsi_Sense_Data(CBW.bLUN, ILLEGAL_REQUEST, PARAMETER_LIST_LENGTH_ERROR);
    Set_CSW(CSW_CMD_FAILED, SEND_CSW_DISABLE);
    return;
  }

  /* READ10/WRITE10 需要提前解析 LBA 与传输块数 */
  if ((CBW.CB[0] == SCSI_READ10) || (CBW.CB[0] == SCSI_WRITE10))
  {
    /* Logical Block Address */
    SCSI_LBA = (CBW.CB[2] << 24) | (CBW.CB[3] << 16) | (CBW.CB[4] <<  8) | CBW.CB[5];
    /* Number of Blocks to transfer */
    SCSI_BlkLen = (CBW.CB[7] <<  8) | CBW.CB[8];
  }

  if (CBW.dSignature == BOT_CBW_SIGNATURE)
  {
    /* Valid CBW */
    if ((CBW.bLUN > MAX_LUN) || (CBW.bCBLength < 1) || (CBW.bCBLength > 16))
    {
      Bot_Abort(BOTH_DIR);
      Set_Scsi_Sense_Data(CBW.bLUN, ILLEGAL_REQUEST, INVALID_FIELED_IN_COMMAND);
      Set_CSW(CSW_CMD_FAILED, SEND_CSW_DISABLE);
    }
    else
    {
      switch (CBW.CB[0])
      {
        /* === B1: 查询命令 (恢复) === */
        case SCSI_REQUEST_SENSE:
          SCSI_RequestSense_Cmd(CBW.bLUN);
          break;
        case SCSI_INQUIRY:
          SCSI_Inquiry_Cmd(CBW.bLUN);
          break;
        case SCSI_START_STOP_UNIT:
          SCSI_Start_Stop_Unit_Cmd(CBW.bLUN);
          break;
        case SCSI_ALLOW_MEDIUM_REMOVAL:
          SCSI_Start_Stop_Unit_Cmd(CBW.bLUN);
          break;
        case SCSI_MODE_SENSE6:
          SCSI_ModeSense6_Cmd(CBW.bLUN);
          break;
        case SCSI_MODE_SENSE10:
          SCSI_ModeSense10_Cmd(CBW.bLUN);
          break;
        case SCSI_READ_FORMAT_CAPACITIES:
          SCSI_ReadFormatCapacity_Cmd(CBW.bLUN);
          break;
        case SCSI_READ_CAPACITY10:
          SCSI_ReadCapacity10_Cmd(CBW.bLUN);
          break;
        case SCSI_TEST_UNIT_READY:
          SCSI_TestUnitReady_Cmd(CBW.bLUN);
          break;
        /* === B1: 不支持命令 (宏别名 → SCSI_Invalid_Cmd) === */
        case SCSI_MODE_SELECT10:
          SCSI_Mode_Select10_Cmd(CBW.bLUN);
          break;
        case SCSI_MODE_SELECT6:
          SCSI_Mode_Select6_Cmd(CBW.bLUN);
          break;
        case SCSI_SEND_DIAGNOSTIC:
          SCSI_Send_Diagnostic_Cmd(CBW.bLUN);
          break;
        case SCSI_READ6:
          SCSI_Read6_Cmd(CBW.bLUN);
          break;
        case SCSI_READ12:
          SCSI_Read12_Cmd(CBW.bLUN);
          break;
        case SCSI_READ16:
          SCSI_Read16_Cmd(CBW.bLUN);
          break;
        case SCSI_READ_CAPACITY16:
          SCSI_READ_CAPACITY16_Cmd(CBW.bLUN);
          break;
        case SCSI_WRITE6:
          SCSI_Write6_Cmd(CBW.bLUN);
          break;
        case SCSI_WRITE12:
          SCSI_Write12_Cmd(CBW.bLUN);
          break;
        case SCSI_WRITE16:
          SCSI_Write16_Cmd(CBW.bLUN);
          break;
        case SCSI_VERIFY12:
          SCSI_Verify12_Cmd(CBW.bLUN);
          break;
        case SCSI_VERIFY16:
          SCSI_Verify16_Cmd(CBW.bLUN);
          break;
#if 0  /* === 数据命令 (C1/C2/C3 恢复) === */
        case SCSI_READ10:
          SCSI_Read10_Cmd(CBW.bLUN, SCSI_LBA, SCSI_BlkLen);
          break;
        case SCSI_WRITE10:
          SCSI_Write10_Cmd(CBW.bLUN, SCSI_LBA, SCSI_BlkLen);
          break;
        case SCSI_VERIFY10:
          SCSI_Verify10_Cmd(CBW.bLUN);
          break;
        case SCSI_FORMAT_UNIT:
          SCSI_Format_Cmd(CBW.bLUN);
          break;
#endif
        default:
          Bot_Abort(BOTH_DIR);
          Set_Scsi_Sense_Data(CBW.bLUN, ILLEGAL_REQUEST, INVALID_COMMAND);
          Set_CSW(CSW_CMD_FAILED, SEND_CSW_DISABLE);
          break;
      }
    }
  }
  else
  {
    /* Invalid CBW */
    Bot_Abort(BOTH_DIR);
    /* A3 删除: Set_Scsi_Sense_Data(CBW.bLUN, ILLEGAL_REQUEST, INVALID_COMMAND); */
    Set_CSW(CSW_CMD_FAILED, SEND_CSW_DISABLE);
  }
}

/*******************************************************************************
* Function Name  : Transfer_Data_Request
* Description    : SCSI 查询命令通过本函数把响应数据发到 EP1 IN。
*                  发完后 Bot_State = BOT_DATA_IN_LAST，下一次 IN 中断
*                  由 Mass_Storage_In() 进入 Set_CSW。
* Input          : Data_Pointer - 待发送数据指针
*                  Data_Len     - 字节数
* Output         : None.
* Return         : None.
*******************************************************************************/
void Transfer_Data_Request(uint8_t* Data_Pointer, uint16_t Data_Len)
{
  USB_SIL_Write(EP1_IN, Data_Pointer, Data_Len);
  SetEPTxStatus(ENDP1, EP_TX_VALID);

  Bot_State = BOT_DATA_IN_LAST;
  CSW.dDataResidue -= Data_Len;
  CSW.bStatus = CSW_CMD_PASSED;
}

/*******************************************************************************
* Function Name  : Set_CSW
* Description    : 填充 CSW 并 (可选) 立即发送。
*                  - Send_Permission = SEND_CSW_ENABLE: 立即触发 IN 发送，
*                    Bot_State = BOT_CSW_Send，下一次 IN 中断回到 IDLE。
*                  - Send_Permission = SEND_CSW_DISABLE: 只准备 CSW 不发送，
*                    Bot_State = BOT_ERROR，等待 Mass Storage Reset 后再发。
* Input          : CSW_Status      - CSW_CMD_PASSED / CSW_CMD_FAILED / CSW_PHASE_ERROR
*                  Send_Permission - SEND_CSW_ENABLE / SEND_CSW_DISABLE
* Output         : None.
* Return         : None.
*******************************************************************************/
void Set_CSW(uint8_t CSW_Status, uint8_t Send_Permission)
{
  CSW.dSignature = BOT_CSW_SIGNATURE;
  CSW.bStatus = CSW_Status;

  USB_SIL_Write(EP1_IN, ((uint8_t *)&CSW), CSW_DATA_LENGTH);

  Bot_State = BOT_ERROR;
  if (Send_Permission)
  {
    Bot_State = BOT_CSW_Send;
    SetEPTxStatus(ENDP1, EP_TX_VALID);
  }
}

/*******************************************************************************
* Function Name  : Bot_Abort
* Description    : 按方向 Stall 端点，用于错误恢复。
* Input          : Direction - DIR_IN / DIR_OUT / BOTH_DIR
* Output         : None.
* Return         : None.
*******************************************************************************/
void Bot_Abort(uint8_t Direction)
{
  switch (Direction)
  {
    case DIR_IN:
      SetEPTxStatus(ENDP1, EP_TX_STALL);
      break;
    case DIR_OUT:
      SetEPRxStatus(ENDP2, EP_RX_STALL);
      break;
    case BOTH_DIR:
      SetEPTxStatus(ENDP1, EP_TX_STALL);
      SetEPRxStatus(ENDP2, EP_RX_STALL);
      break;
    default:
      break;
  }
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
