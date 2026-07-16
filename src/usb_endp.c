/**
  ******************************************************************************
  * @file    usb_endp.c
  * @brief   端点回调 — EP1 IN / EP2 OUT 转发到 BOT 状态机
  ******************************************************************************
  */

#include "usb_lib.h"
#include "usb_bot.h"

/* ── EP1 IN 回调 — CSW / READ 数据发送完成 ────────────────────────────────── */
void EP1_IN_Callback(void)
{
    Mass_Storage_In();
}

/* ── EP2 OUT 回调 — 接收 CBW / WRITE 数据 ────────────────────────────────── */
void EP2_OUT_Callback(void)
{
    Mass_Storage_Out();
}
