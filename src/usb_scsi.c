/**
  ******************************************************************************
  * @file    usb_scsi.c
  * @brief   SCSI 查询命令实现 (TR2-B1)
  *
  *          B1 阶段只实现 9 个查询命令 + Set_Scsi_Sense_Data + Invalid/Valid_Cmd。
  *          与完成态差异: READ10/WRITE10 (C1/C2)、VERIFY10/FORMAT_UNIT (C3)、
  *          SCSI_Address_Management (C1) 在后续阶段追加。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "usb_scsi.h"
#include "scsi_data.h"
#include "mass_mal.h"      /* MAL_GetStatus / Mass_Block_* */
#include "usb_bot.h"       /* CBW/CSW / Bot_Abort / Set_CSW / Transfer_Data_Request */
#include "usb_lib.h"       /* USB 库 (一致性) */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* External variables --------------------------------------------------------*/
extern Bulk_Only_CBW CBW;
extern Bulk_Only_CSW CSW;
extern uint32_t Mass_Block_Size[2];
extern uint32_t Mass_Block_Count[2];

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/*******************************************************************************
* Function Name  : SCSI_Inquiry_Cmd
* Description    : SCSI Inquiry Command (0x12)。
*                  EVPD 置位返回支持页面列表 (Page00)，否则返回标准查询数据。
*                  B1 单 LUN: 非 0 也回退到 Standard_Inquiry_Data (防御)。
* Input          : lun - 逻辑单元号
* Output         : None.
* Return         : None.
*******************************************************************************/
void SCSI_Inquiry_Cmd(uint8_t lun)
{
  uint8_t *Inquiry_Data;
  uint16_t Inquiry_Data_Length;

  if (CBW.CB[1] & 0x01)      /* EVPD 置位: 返回支持页面列表 */
  {
    Inquiry_Data = Page00_Inquiry_Data;
    Inquiry_Data_Length = 5;
  }
  else
  {
    Inquiry_Data = Standard_Inquiry_Data;   /* 单 LUN, 无 Standard_Inquiry_Data2 */
    if (CBW.CB[4] <= STANDARD_INQUIRY_DATA_LEN)
      Inquiry_Data_Length = CBW.CB[4];
    else
      Inquiry_Data_Length = STANDARD_INQUIRY_DATA_LEN;
  }
  Transfer_Data_Request(Inquiry_Data, Inquiry_Data_Length);
}

/*******************************************************************************
* Function Name  : SCSI_ReadFormatCapacity_Cmd
* Description    : SCSI ReadFormatCapacity Command (0x23)。
*                  填充容量描述符后发送 (大端)。
* Input          : lun - 逻辑单元号
* Output         : None.
* Return         : None.
*******************************************************************************/
void SCSI_ReadFormatCapacity_Cmd(uint8_t lun)
{
  if (MAL_GetStatus(lun) != 0)
  {
    Set_Scsi_Sense_Data(CBW.bLUN, NOT_READY, MEDIUM_NOT_PRESENT);
    Set_CSW(CSW_CMD_FAILED, SEND_CSW_ENABLE);
    Bot_Abort(DIR_IN);
    return;
  }
  ReadFormatCapacity_Data[4]  = (uint8_t)(Mass_Block_Count[lun] >> 24);
  ReadFormatCapacity_Data[5]  = (uint8_t)(Mass_Block_Count[lun] >> 16);
  ReadFormatCapacity_Data[6]  = (uint8_t)(Mass_Block_Count[lun] >>  8);
  ReadFormatCapacity_Data[7]  = (uint8_t)(Mass_Block_Count[lun]);
  ReadFormatCapacity_Data[9]  = (uint8_t)(Mass_Block_Size[lun] >>  16);
  ReadFormatCapacity_Data[10] = (uint8_t)(Mass_Block_Size[lun] >>   8);
  ReadFormatCapacity_Data[11] = (uint8_t)(Mass_Block_Size[lun]);
  Transfer_Data_Request(ReadFormatCapacity_Data, READ_FORMAT_CAPACITY_DATA_LEN);
}

/*******************************************************************************
* Function Name  : SCSI_ReadCapacity10_Cmd
* Description    : SCSI ReadCapacity10 Command (0x25)。
*                  填充最后 LBA 与块长 (大端) 后发送。
* Input          : lun - 逻辑单元号
* Output         : None.
* Return         : None.
*******************************************************************************/
void SCSI_ReadCapacity10_Cmd(uint8_t lun)
{
  if (MAL_GetStatus(lun))
  {
    Set_Scsi_Sense_Data(CBW.bLUN, NOT_READY, MEDIUM_NOT_PRESENT);
    Set_CSW(CSW_CMD_FAILED, SEND_CSW_ENABLE);
    Bot_Abort(DIR_IN);
    return;
  }
  /* 最后 LBA (0-based) */
  ReadCapacity10_Data[0] = (uint8_t)((Mass_Block_Count[lun] - 1) >> 24);
  ReadCapacity10_Data[1] = (uint8_t)((Mass_Block_Count[lun] - 1) >> 16);
  ReadCapacity10_Data[2] = (uint8_t)((Mass_Block_Count[lun] - 1) >>  8);
  ReadCapacity10_Data[3] = (uint8_t)(Mass_Block_Count[lun] - 1);
  /* 块长 512 */
  ReadCapacity10_Data[4] = (uint8_t)(Mass_Block_Size[lun] >> 24);
  ReadCapacity10_Data[5] = (uint8_t)(Mass_Block_Size[lun] >> 16);
  ReadCapacity10_Data[6] = (uint8_t)(Mass_Block_Size[lun] >>  8);
  ReadCapacity10_Data[7] = (uint8_t)(Mass_Block_Size[lun]);
  Transfer_Data_Request(ReadCapacity10_Data, READ_CAPACITY10_DATA_LEN);
}

/*******************************************************************************
* Function Name  : SCSI_ModeSense6_Cmd
* Description    : SCSI ModeSense6 Command (0x1A)。
*                  发送预填充的 6 字节模式数据。
* Input          : lun - 逻辑单元号
* Output         : None.
* Return         : None.
*******************************************************************************/
void SCSI_ModeSense6_Cmd(uint8_t lun)
{
  Transfer_Data_Request(Mode_Sense6_data, MODE_SENSE6_DATA_LEN);
}

/*******************************************************************************
* Function Name  : SCSI_ModeSense10_Cmd
* Description    : SCSI ModeSense10 Command (0x5A)。
*                  发送预填充的 10 字节模式数据。
* Input          : lun - 逻辑单元号
* Output         : None.
* Return         : None.
*******************************************************************************/
void SCSI_ModeSense10_Cmd(uint8_t lun)
{
  Transfer_Data_Request(Mode_Sense10_data, MODE_SENSE10_DATA_LEN);
}

/*******************************************************************************
* Function Name  : SCSI_RequestSense_Cmd
* Description    : SCSI RequestSense Command (0x03)。
*                  按 CBW.CB[4] 请求长度发送 Sense 数据。
* Input          : lun - 逻辑单元号
* Output         : None.
* Return         : None.
*******************************************************************************/
void SCSI_RequestSense_Cmd(uint8_t lun)
{
  uint8_t Request_Sense_data_Length;

  if (CBW.CB[4] <= REQUEST_SENSE_DATA_LEN)
    Request_Sense_data_Length = CBW.CB[4];
  else
    Request_Sense_data_Length = REQUEST_SENSE_DATA_LEN;

  Transfer_Data_Request(Scsi_Sense_Data, Request_Sense_data_Length);
}

/*******************************************************************************
* Function Name  : Set_Scsi_Sense_Data
* Description    : 设置 Sense Data 的 Sense Key 与 Additional Sense Code。
* Input          : lun - 逻辑单元号; Sens_Key - Sense Key; Asc - ASC
* Output         : None.
* Return         : None.
*******************************************************************************/
void Set_Scsi_Sense_Data(uint8_t lun, uint8_t Sens_Key, uint8_t Asc)
{
  Scsi_Sense_Data[2]  = Sens_Key;   /* Sense Key */
  Scsi_Sense_Data[12] = Asc;        /* Additional Sense Code */
}

/*******************************************************************************
* Function Name  : SCSI_Start_Stop_Unit_Cmd
* Description    : SCSI Start/Stop Unit (0x1B) 与 ALLOW_MEDIUM_REMOVAL (0x1E)
*                  共用：SRAM 介质无需启停，直接返回成功。
* Input          : lun - 逻辑单元号
* Output         : None.
* Return         : None.
*******************************************************************************/
void SCSI_Start_Stop_Unit_Cmd(uint8_t lun)
{
  Set_CSW(CSW_CMD_PASSED, SEND_CSW_ENABLE);
}

/*******************************************************************************
* Function Name  : SCSI_TestUnitReady_Cmd
* Description    : SCSI TestUnitReady Command (0x00)。
*                  介质就绪返回成功，否则 NOT_READY/MEDIUM_NOT_PRESENT。
* Input          : lun - 逻辑单元号
* Output         : None.
* Return         : None.
*******************************************************************************/
void SCSI_TestUnitReady_Cmd(uint8_t lun)
{
  if (MAL_GetStatus(lun))
  {
    Set_Scsi_Sense_Data(CBW.bLUN, NOT_READY, MEDIUM_NOT_PRESENT);
    Set_CSW(CSW_CMD_FAILED, SEND_CSW_ENABLE);
    Bot_Abort(DIR_IN);
    return;
  }
  Set_CSW(CSW_CMD_PASSED, SEND_CSW_ENABLE);
}

/*******************************************************************************
* Function Name  : SCSI_Invalid_Cmd
* Description    : 不支持命令的统一处理：按方向 Stall + 返回 CSW_CMD_FAILED。
* Input          : lun - 逻辑单元号
* Output         : None.
* Return         : None.
*******************************************************************************/
void SCSI_Invalid_Cmd(uint8_t lun)
{
  if (CBW.dDataLength == 0)
  {
    Bot_Abort(DIR_IN);
  }
  else
  {
    if ((CBW.bmFlags & 0x80) != 0)
      Bot_Abort(DIR_IN);
    else
      Bot_Abort(BOTH_DIR);
  }
  Set_Scsi_Sense_Data(CBW.bLUN, ILLEGAL_REQUEST, INVALID_COMMAND);
  Set_CSW(CSW_CMD_FAILED, SEND_CSW_DISABLE);
}

/*******************************************************************************
* Function Name  : SCSI_Valid_Cmd
* Description    : 无数据传输的合法命令统一处理。
* Input          : lun - 逻辑单元号
* Output         : None.
* Return         : None.
*******************************************************************************/
void SCSI_Valid_Cmd(uint8_t lun)
{
  if (CBW.dDataLength != 0)
  {
    Bot_Abort(BOTH_DIR);
    Set_Scsi_Sense_Data(CBW.bLUN, ILLEGAL_REQUEST, INVALID_COMMAND);
    Set_CSW(CSW_CMD_FAILED, SEND_CSW_DISABLE);
  }
  else
    Set_CSW(CSW_CMD_PASSED, SEND_CSW_ENABLE);
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
