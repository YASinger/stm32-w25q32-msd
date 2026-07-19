#include "usb_pwr.h"
#include "hw_config.h"
#include "usb_lib.h"

/* ── 全局状态 ────────────────────────────────────────────────────────────── */
__IO uint32_t bDeviceState = UNCONNECTED;
__IO bool     fSuspendEnabled = FALSE;   /* TR1 阶段禁用挂起, 避免 SUSP 中断进入 STOP 导致枚举失败 */
__IO uint32_t remotewakeupon = 0;

static struct {
    __IO RESUME_STATE eState;
    __IO uint8_t bESOFcnt;
} ResumeS;

/* ── PowerOn: 上电, 使能 D+ 上拉 ──────────────────────────────────────────── */
void PowerOn(void)
{
    /* [1] D+ 上拉使能 — PA12 切回 AF_PP, 主机检测到全速设备插入 */
    USB_Cable_Config(ENABLE);

    /* [2] 强制复位 USB 外设, 清除 USB_SIL_Init 期间可能残留的状态 */
    SetCNTR(CNTR_FRES);

    /* [3] 清除强制复位 */
    SetCNTR(0);

    /* [4] 清除挂起的中断标志 + 使能 USB 中断 */
    SetISTR(0);
    wInterrupt_Mask = IMR_MSK;
    SetCNTR(IMR_MSK);

    /* [5] 状态 → 已连接 */
    bDeviceState = ATTACHED;
}

/* ── PowerOff: 断电, 断开 D+ 上拉 ─────────────────────────────────────────── */
void PowerOff(void)
{
    SetCNTR(CNTR_FRES);               /* 强制 USB 复位 */
    SetISTR(0);                       /* 清中断标志 */
    USB_Cable_Config(DISABLE);        /* PA12 输出低 → D+ 拉低 */
    SetCNTR(CNTR_FRES + CNTR_PDWN);   /* USB 外设断电, 释放 PA12 控制权 */
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
