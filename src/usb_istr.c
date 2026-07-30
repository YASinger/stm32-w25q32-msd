/**
  ******************************************************************************
  * @file    usb_istr.c
  * @brief   USB 中断服务例程 — 提供 USB_Istr() 及端点回调表
  ******************************************************************************
  */

#include "usb_lib.h"
#include "usb_pwr.h"
#include "usb_istr.h"   /* TR2-A3: EP1_IN_Callback / EP2_OUT_Callback 函数原型 */

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

#if (IMR_MSK & ISTR_SOF)
    if (wIstr & ISTR_SOF & wInterrupt_Mask) {
        _SetISTR((uint16_t)CLR_SOF);
        bIntPackSOF++;
    }
#endif

#if (IMR_MSK & ISTR_ESOF)
    if (wIstr & ISTR_ESOF & wInterrupt_Mask) {
        _SetISTR((uint16_t)CLR_ESOF);
        Resume(RESUME_ESOF);
    }
#endif
}

void USB_LP_CAN1_RX0_IRQHandler(void)
{
    USB_Istr();
}
