#include "stm32f10x.h"                  // Device header
#include "hw_config.h"
#include "usb_lib.h"
#include "usb_pwr.h"

static void Delay(__IO uint32_t nCount)
{
  for(; nCount != 0; nCount--);
}

/* PC13 LED: 低电平点亮 (C8T6 最小系统板) */
#define LED_ON()    GPIO_ResetBits(GPIOC, GPIO_Pin_13)
#define LED_OFF()   GPIO_SetBits(GPIOC, GPIO_Pin_13)

int main(void)
{
  Set_System();
  Set_USBClock();
  USB_Interrupts_Config();
  Get_SerialNum();              /* C2: 用 MCU UID 填充序列号字符串 */
  USB_Init();                  /* B2: 调用 MASS_init() → USB_SIL_Init() */
  PowerOn();                   /* B3: D+ 上拉使能 + USB 外设复位 + 中断使能 */

  /* 初始化 PC13 LED */
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
  GPIO_InitTypeDef GPIO_InitStructure;
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_13;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOC, &GPIO_InitStructure);
  LED_OFF();                   /* 首次枚举等待期间 LED 灭 */

  while (bDeviceState != CONFIGURED);  /* B4: 等待首次枚举完成 (C3 后退出) */

  LED_ON();                    /* 枚举成功, LED 亮 */

  while (1)
  {

  }
}
