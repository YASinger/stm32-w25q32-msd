#include "hw_config.h"

void Set_System(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* ① 使能 GPIOA 时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    /* ② PA12 初始化为 GPIO 推挽输出低 — D+ 拉低, 主机认为设备断开 */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_ResetBits(GPIOA, GPIO_Pin_12);

    /* ③ PA11 初始化为 USB 复用推挽输出 — DM 就绪 */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

void Set_USBClock(void)
{
    /* USB 时钟 = PLLCLK ÷ 1.5 = 72 ÷ 1.5 = 48MHz */
    RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_1Div5);

    /* 使能 USB 外设时钟 */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USB, ENABLE);
}

void USB_Interrupts_Config(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;

    /* 优先级分组: 2位抢占, 2位响应 */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    /* USB 低优先级中断 (RESET / CTR / SUSP / WKUP / SOF / ERR) */
    NVIC_InitStructure.NVIC_IRQChannel = USB_LP_CAN1_RX0_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* USB 唤醒中断 */
    NVIC_InitStructure.NVIC_IRQChannel = USBWakeUp_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

void USB_Cable_Config(FunctionalState NewState)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    if (NewState == DISABLE) {
        /* 断开: PA12 切 GPIO 输出低 → D+ 通过 R6(22Ω) 拉到 GND */
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
        GPIO_Init(GPIOA, &GPIO_InitStructure);
        GPIO_ResetBits(GPIOA, GPIO_Pin_12);
    } else {
        /* 连接: PA12 恢复 AF_PP → D+ 被 R10(1.5kΩ) 上拉到 3.3V */
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
        GPIO_Init(GPIOA, &GPIO_InitStructure);
    }
}
