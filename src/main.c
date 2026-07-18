#include "stm32f10x.h"                  // Device header
#include "hw_config.h"
#include "usb_lib.h"
#include "usb_pwr.h"

int main(void)
{
  Set_System();
  Set_USBClock();
  USB_Interrupts_Config();
  USB_Init();                  /* B2: 调用 MASS_init() → USB_SIL_Init() */
  PowerOn();                   /* B3: D+ 上拉使能 + USB 外设复位 + 中断使能 */

  while (bDeviceState != CONFIGURED);  /* B4: 等待枚举完成 (C3 后退出) */

  while (1)
  {
  }
}
