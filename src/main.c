#include "stm32f10x.h"                  // Device header
#include "hw_config.h"
#include "usb_lib.h"

int main(void)
{
  Set_System();
  Set_USBClock();
  USB_Interrupts_Config();
  USB_Init();                  /* B2 新增: 调用 MASS_init() → USB_SIL_Init() */
  USB_Cable_Config(ENABLE);    /* PA12 切为 AF_PP, D+ 上拉生效 */

  while (1)
  {
  }
}
