#include "stm32f10x.h"
#include "hw_config.h"
#include "usb_lib.h"
#include "usb_pwr.h"

int main(void)
{
	Set_System();                   // 系统时钟 + GPIO (PA11 AF_PP, PA12 初始输出低)
	Set_USBClock();                 // USB 48MHz
	USB_Interrupts_Config();        // NVIC
	USB_Init();                     // 内部调用 PowerOn → USB_Cable_Config(ENABLE) → PA12 切 AF_PP
	
	while (bDeviceState != CONFIGURED);  // 等待枚举完成
	while (1) {}                    // 中断驱动主循环
}
