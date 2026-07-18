#include "stm32f10x.h"                  // Device header
#include "hw_config.h"

int main(void)
{
  Set_System();
  Set_USBClock();
  USB_Interrupts_Config();
  USB_Cable_Config(ENABLE);   /* PA12 切为 AF_PP, 满足 A2 验收 */

  while (1)
  {
  }
}
