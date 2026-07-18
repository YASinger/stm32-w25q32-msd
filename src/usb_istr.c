/**
  ******************************************************************************
  * @file    usb_istr.c
  * @brief   USB 中断服务例程 — TR1-A4
  *
  *          中断入口 + ISTR 事件分发 + 端点回调表。
  *          以 tmp\last_project 完成态为基准，A4 阶段因 usb_pwr.h (B3)
  *          尚未创建，裁剪其依赖：删除 #include "usb_pwr.h"，
  *          WKUP/SUSP/ESOF 三个分支 #if 0 包裹，B3 完成后恢复。
  ******************************************************************************
  */

#include "usb_lib.h"

__IO uint16_t wIstr;
__IO uint8_t  bIntPackSOF = 0;

/* 端点回调表 — TR1 阶段全部指向 NOP_Process */
void (*pEpInt_IN[7])(void) = {
    EP1_IN_Callback,
    EP2_IN_Callback,
    EP3_IN_Callback,
    EP4_IN_Callback,
    EP5_IN_Callback,
    EP6_IN_Callback,
    EP7_IN_Callback,
};

void (*pEpInt_OUT[7])(void) = {
    EP1_OUT_Callback,
    EP2_OUT_Callback,
    EP3_OUT_Callback,
    EP4_OUT_Callback,
    EP5_OUT_Callback,
    EP6_OUT_Callback,
    EP7_OUT_Callback,
};

void USB_Istr(void)
{
    wIstr = _GetISTR();

#if (IMR_MSK & ISTR_CTR)
    if (wIstr & ISTR_CTR & wInterrupt_Mask) {
        CTR_LP();
    }
#endif

#if (IMR_MSK & ISTR_RESET)
    if (wIstr & ISTR_RESET & wInterrupt_Mask) {
        _SetISTR((uint16_t)CLR_RESET);
        Device_Property.Reset();
    }
#endif

#if (IMR_MSK & ISTR_DOVR)
    if (wIstr & ISTR_DOVR & wInterrupt_Mask) {
        _SetISTR((uint16_t)CLR_DOVR);
    }
#endif

#if (IMR_MSK & ISTR_ERR)
    if (wIstr & ISTR_ERR & wInterrupt_Mask) {
        _SetISTR((uint16_t)CLR_ERR);
    }
#endif

#if 0  /* === 以下三个分支依赖 usb_pwr.h (B3)，A4 阶段暂不编译 === */

#if (IMR_MSK & ISTR_WKUP)
    if (wIstr & ISTR_WKUP & wInterrupt_Mask) {
        _SetISTR((uint16_t)CLR_WKUP);
        Resume(RESUME_EXTERNAL);
    }
#endif

#if (IMR_MSK & ISTR_SUSP)
    if (wIstr & ISTR_SUSP & wInterrupt_Mask) {
        if (fSuspendEnabled) {
            Suspend();
        } else {
            Resume(RESUME_LATER);
        }
        _SetISTR((uint16_t)CLR_SUSP);
    }
#endif

#if (IMR_MSK & ISTR_ESOF)
    if (wIstr & ISTR_ESOF & wInterrupt_Mask) {
        _SetISTR((uint16_t)CLR_ESOF);
        Resume(RESUME_ESOF);
    }
#endif

#endif /* 0 === B3 恢复的分支 === */

#if (IMR_MSK & ISTR_SOF)
    if (wIstr & ISTR_SOF & wInterrupt_Mask) {
        _SetISTR((uint16_t)CLR_SOF);
        bIntPackSOF++;
    }
#endif
}

void USB_LP_CAN1_RX0_IRQHandler(void)
{
    USB_Istr();
}
