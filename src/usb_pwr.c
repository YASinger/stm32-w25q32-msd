#include "usb_pwr.h"
#include "hw_config.h"
#include "usb_lib.h"

/* ── 全局状态 ────────────────────────────────────────────────────────────── */
__IO uint32_t bDeviceState = UNCONNECTED;
__IO bool     fSuspendEnabled = TRUE;
__IO uint32_t remotewakeupon = 0;

static struct {
    __IO RESUME_STATE eState;
    __IO uint8_t bESOFcnt;
} ResumeS;

/* ── PowerOn: 上电, 使能 D+ 上拉 ──────────────────────────────────────────── */
void PowerOn(void)
{
    USB_Cable_Config(ENABLE);                   /* PA12 切 AF_PP → D+ 上拉使能 */

    SetCNTR(CNTR_FRES);                         /* 强制复位 USB 外设           */
    SetCNTR(0);                                 /* 清除复位                     */
    SetISTR(0);                                 /* 清除所有挂起的中断标志       */
    wInterrupt_Mask = IMR_MSK;
    SetCNTR(IMR_MSK);                           /* 使能 USB 中断                */

    bDeviceState = ATTACHED;                    /* 状态 → 已连接                */
}

/* ── PowerOff: 断电, 断开 D+ 上拉 ─────────────────────────────────────────── */
void PowerOff(void)
{
    USB_Cable_Config(DISABLE);                  /* PA12 输出低 → D+ 拉低       */
    bDeviceState = UNCONNECTED;
}

/* ── Suspend: USB 挂起 ────────────────────────────────────────────────────── */
void Suspend(void)
{
    uint16_t wCNTR;

    /* 暂停状态时允许退出 PHY 时钟 */
    wCNTR = GetCNTR();
    wCNTR |= CNTR_FSUSP;
    SetCNTR(wCNTR);

    /* 降低功耗: 关闭模拟部分 */
    wCNTR = GetCNTR();
    wCNTR |= CNTR_LPMODE;
    SetCNTR(wCNTR);

    /* 进入停止模式 */
    PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);

    /* 唤醒后恢复 USB 时钟 */
    SetCNTR(IMR_MSK);
    SetISTR(0);
    bDeviceState = SUSPENDED;
}

/* ── Resume: USB 唤醒 ─────────────────────────────────────────────────────── */
void Resume(RESUME_STATE eResumeSetVal)
{
    uint16_t wCNTR;

    switch (eResumeSetVal) {
    case RESUME_ESOF:
        ResumeS.eState = RESUME_ESOF;
        break;

    case RESUME_EXTERNAL:
        ResumeS.eState = RESUME_EXTERNAL;
        break;

    case RESUME_INTERNAL:
        ResumeS.eState = RESUME_INTERNAL;
        break;

    case RESUME_LATER:
        /* 延迟唤醒: 遥控唤醒功能 */
        if (remotewakeupon == 0) {
            ResumeS.eState = RESUME_OFF;
        } else {
            ResumeS.eState = RESUME_ON;
        }
        break;

    case RESUME_WAIT:
        ResumeS.eState = RESUME_WAIT;
        break;

    case RESUME_ON:
        wCNTR = GetCNTR();
        wCNTR |= CNTR_RESUME;
        SetCNTR(wCNTR);

        ResumeS.eState = RESUME_ON;
        ResumeS.bESOFcnt = 10;
        break;

    default:
        break;
    }
}
