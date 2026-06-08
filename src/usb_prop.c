/**
  ******************************************************************************
  * @file    usb_prop.c
  * @brief   USB 设备属性（最小桩版本）
  *           提供 USB 核心库需要的 Device_Table、Device_Property、
  *           User_Standard_Requests 定义。所有回调初始化为 NULL，
  *           实际使用时需替换为真实实现。
  ******************************************************************************
  */

#include "usb_lib.h"

/* 端点配置表 */
DEVICE Device_Table = {
    0,  /* Total_Endpoint — 0 个端点（最小配置） */
    1   /* Total_Configuration — 1 个配置 */
};

/* 设备属性回调表 */
DEVICE_PROP Device_Property = {
    NULL,  /* Init */
    NULL,  /* Reset */
    NULL,  /* Process_Status_IN */
    NULL,  /* Process_Status_OUT */
    NULL,  /* Class_Data_Setup */
    NULL,  /* Class_NoData_Setup */
    NULL,  /* Class_Get_Interface_Setting */
    NULL,  /* GetDeviceDescriptor */
    NULL,  /* GetConfigDescriptor */
    NULL,  /* GetStringDescriptor */
    NULL,  /* RxEP_buffer */
    64     /* MaxPacketSize = 64 字节（全速 USB） */
};

/* USB 标准请求回调表 */
USER_STANDARD_REQUESTS User_Standard_Requests = {
    NULL,  /* User_GetConfiguration */
    NULL,  /* User_SetConfiguration */
    NULL,  /* User_GetInterface */
    NULL,  /* User_SetInterface */
    NULL,  /* User_GetStatus */
    NULL,  /* User_ClearFeature */
    NULL,  /* User_SetEndPointFeature */
    NULL,  /* User_SetDeviceFeature */
    NULL   /* User_SetDeviceAddress */
};
