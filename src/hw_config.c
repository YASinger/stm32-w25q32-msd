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
#include "usb_desc.h"

/* MCU 96-bit 唯一 ID 寄存器地址 (STM32F10x) */
#define ID1     (0x1FFFF7E8)
#define ID2     (0x1FFFF7EC)
#define ID3     (0x1FFFF7F0)

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

/*******************************************************************************
* Function Name  : IntToUnicode
* Description    : 32 位值转 len 位十六进制 Unicode 字符
* Input          : value - 32 位值
*                  pbuf  - 输出缓冲区 (每字符 2 字节: ASCII + 0x00)
*                  len   - 十六进制位数
* Return         : None
*******************************************************************************/
static void IntToUnicode(uint32_t value, uint8_t *pbuf, uint8_t len)
{
  uint8_t idx;
  for (idx = 0; idx < len; idx++) {
    if ((value >> 28) < 0xA) {
      pbuf[2 * idx] = (uint8_t)((value >> 28) + '0');
    } else {
      pbuf[2 * idx] = (uint8_t)((value >> 28) + 'A' - 10);
    }
    value <<= 4;
    pbuf[2 * idx + 1] = 0;
  }
}

/*******************************************************************************
* Function Name  : Get_SerialNum
* Description    : 用 MCU 96-bit UID 填充序列号字符串 (12 位十六进制)
* Input          : None
* Return         : None
* Note           : 读 3 个 UID 寄存器, 混合后转 12 位十六进制填入
*                  MASS_StringSerial。每块板子序列号不同。
*******************************************************************************/
void Get_SerialNum(void)
{
  uint32_t Device_Serial0, Device_Serial1, Device_Serial2;

  Device_Serial0 = *(uint32_t *)ID1;
  Device_Serial1 = *(uint32_t *)ID2;
  Device_Serial2 = *(uint32_t *)ID3;

  Device_Serial0 += Device_Serial2;

  if (Device_Serial0 != 0) {
    IntToUnicode(Device_Serial0, &MASS_StringSerial[2], 8);
    IntToUnicode(Device_Serial1, &MASS_StringSerial[18], 4);
  }
}
