/**
  ******************************************************************************
  * @file    usb_endp.c
  * @brief   端点回调接管 (TR2-A3)
  *
  *          TR1 阶段 usb_conf.h 中将 EP1_IN_Callback / EP2_OUT_Callback
  *          宏定义为 NOP_Process。本文件配合 usb_conf.h 中宏的注释掉，
  *          提供同名函数定义，把端点中断真正接到 BOT 状态机。
  *
  *          - EP1_IN_Callback  -> Mass_Storage_In()   (CSW/数据 IN 发送完成)
  *          - EP2_OUT_Callback -> Mass_Storage_Out()  (CBW/数据 OUT 接收完成)
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "usb_lib.h"
#include "usb_bot.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/*******************************************************************************
* Function Name  : EP1_IN_Callback
* Description    : EP1 IN 回调 — Bulk IN 传输完成。
*                  转发到 usb_bot.c 的 Mass_Storage_In() 推进状态机。
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
void EP1_IN_Callback(void)
{
  Mass_Storage_In();
}

/*******************************************************************************
* Function Name  : EP2_OUT_Callback
* Description    : EP2 OUT 回调 — Bulk OUT 接收完成。
*                  转发到 usb_bot.c 的 Mass_Storage_Out() 推进状态机。
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
void EP2_OUT_Callback(void)
{
  Mass_Storage_Out();
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
