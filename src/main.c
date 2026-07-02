#include "stm32f10x.h"
#include "hw_config.h"
#include "usb_lib.h"
#include "usb_pwr.h"

int main(void)
{
	Set_System();                   // 系统时钟 + GPIO (PA11 AF_PP, PA12 初始输出低)
	Set_USBClock();                 // USB 48MHz
	USB_Interrupts_Config();        // NVIC
	USB_Init();                     // 初始化设备表、调用 MASS_init() → USB_SIL_Init()

	/* ── LED 初始化: PC13 推挽输出, 初始灭 (低电平点亮) ── */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	GPIO_SetBits(GPIOC, GPIO_Pin_13);  // LED 初始灭

	PowerOn();                      // D+ 上拉使能 + USB 外设复位 + 中断使能 (无延时)

	while (bDeviceState != CONFIGURED);  // 等待枚举完成

	GPIO_ResetBits(GPIOC, GPIO_Pin_13);  // LED 亮 — 枚举成功 (CONFIGURED)

	while (1) {}                    // 中断驱动主循环
}
