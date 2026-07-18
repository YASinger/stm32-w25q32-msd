/**
  ******************************************************************************
  * @file    hw_config.c
  * @brief   硬件配置实现 — TR1-A2
  *
  *          实现 USB 外设物理可用所需的全部硬件初始化：
  *          GPIO 时钟、PA11/PA12 配置、USB 48MHz 时钟、NVIC 中断。
  *
  *          裁剪自 ST 官方 hw_config.c，仅保留 STM32F103C8 +
  *          无 PMOS 上拉开关 + 禁用挂起模式所需的最小实现。
  *          多芯片 #ifdef、EVAL 板 LED、SDIO、EXTI 唤醒、
  *          MAL_Config、Get_SerialNum 等已删除。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "hw_config.h"

/* Private variables ---------------------------------------------------------*/
ErrorStatus HSEStartUpStatus;

/*******************************************************************************
* Function Name  : Set_System
* Description    : Configures GPIO clock and USB DM/DP pins (PA11/PA12)
* Input          : None
* Return         : None
* Note           : 系统时钟 72MHz 已由启动文件 SystemInit() 配置，此处不重做。
*                  本板无 PMOS 上拉开关，PA12 初始设为 Out_PP 输出低，
*                  让主机认为设备未连接，待 USB_Cable_Config(ENABLE) 切 AF_PP。
*******************************************************************************/
void Set_System(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;

  /* 使能 GPIOA 时钟 */
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

  /* PA11 (USB DM) 配置为复用推挽 — DM 固定复用，直接 AF_PP */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  /* PA12 (USB DP) 先设为普通推挽输出低，拉低 D+ 模拟设备未连接 */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  GPIO_ResetBits(GPIOA, GPIO_Pin_12);
}

/*******************************************************************************
* Function Name  : Set_USBClock
* Description    : Configures USB Clock input (48MHz from PLLCLK / 1.5)
* Input          : None
* Return         : None
*******************************************************************************/
void Set_USBClock(void)
{
  /* 选择 USB 时钟源: PLLCLK / 1.5 = 72 / 1.5 = 48MHz */
  RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_1Div5);

  /* 使能 USB 外设时钟 */
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USB, ENABLE);
}

/*******************************************************************************
* Function Name  : USB_Interrupts_Config
* Description    : Configures the USB interrupts (NVIC Group 2, Preemption 0)
* Input          : None
* Return         : None
* Note           : USBWakeUp 中断明确禁用 (TR1 禁用挂起模式, §6.2)
*******************************************************************************/
void USB_Interrupts_Config(void)
{
  NVIC_InitTypeDef NVIC_InitStructure;

  /* 优先级组 2: 2 位抢占优先级, 2 位响应优先级 */
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

  /* USB 低优先级中断 (USB_LP_CAN1_RX0): 抢占 0, 响应 0 */
  NVIC_InitStructure.NVIC_IRQChannel = USB_LP_CAN1_RX0_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);

  /* USB 唤醒中断: 明确禁用 (TR1 禁用挂起模式) */
  NVIC_InitStructure.NVIC_IRQChannel = USBWakeUp_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelCmd = DISABLE;
  NVIC_Init(&NVIC_InitStructure);
}

/*******************************************************************************
* Function Name  : USB_Cable_Config
* Description    : Software Connection/Disconnection of USB Cable
* Input          : NewState - ENABLE (connect) / DISABLE (disconnect)
* Return         : None
* Note           : 本板无 PMOS 开关, 通过 PA12 模式切换实现软件重连。
*                  ENABLE:  PA12 切为 AF_PP, 交由 USB 外设驱动 D+
*                  DISABLE: PA12 切为 Out_PP 输出低, 拉低 D+ 模拟断开
*******************************************************************************/
void USB_Cable_Config(FunctionalState NewState)
{
  GPIO_InitTypeDef GPIO_InitStructure;

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

  if (NewState != DISABLE)
  {
    /* 连接: PA12 切为 AF_PP, 交由 USB 外设 */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
  }
  else
  {
    /* 断开: PA12 切为 Out_PP 输出低, 拉低 D+ */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_ResetBits(GPIOA, GPIO_Pin_12);
  }
}

/*******************************************************************************
* Function Name  : Enter_LowPowerMode
* Description    : Enters low power mode (suspend)
* Input          : None
* Return         : None
* Note           : TR1 禁用挂起模式 (fSuspendEnabled = FALSE), 此函数不会被
*                  调用。提供空实现以满足 usb_pwr.c 的链接依赖。
*******************************************************************************/
void Enter_LowPowerMode(void)
{
  /* TR1 禁用挂起, 空实现 */
}

/*******************************************************************************
* Function Name  : Leave_LowPowerMode
* Description    : Leaves low power mode (resume)
* Input          : None
* Return         : None
* Note           : 同 Enter_LowPowerMode, 空实现。
*******************************************************************************/
void Leave_LowPowerMode(void)
{
  /* TR1 禁用挂起, 空实现 */
}
