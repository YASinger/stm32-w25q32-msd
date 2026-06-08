/**
  ******************************************************************************
  * @file    usb_istr.c
  * @brief   USB 中断服务例程（最小桩版本）
  *           提供 USB 核心库需要的 wIstr、pEpInt_IN、pEpInt_OUT 定义。
  ******************************************************************************
  */

#include "usb_lib.h"

/* ISTR 寄存器缓存值 — usb_int.c 需要使用 */
__IO uint16_t wIstr;

/* 端点 IN 回调表（7 个端点，全部指向空操作） */
void (*pEpInt_IN[7])(void) = {
    NOP_Process, NOP_Process, NOP_Process, NOP_Process,
    NOP_Process, NOP_Process, NOP_Process
};

/* 端点 OUT 回调表（7 个端点，全部指向空操作） */
void (*pEpInt_OUT[7])(void) = {
    NOP_Process, NOP_Process, NOP_Process, NOP_Process,
    NOP_Process, NOP_Process, NOP_Process
};
