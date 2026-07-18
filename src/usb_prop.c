/**
  ******************************************************************************
  * @file    usb_prop.c
  * @brief   USB 设备属性 — TR1-B2
  *
  *          Device_Property 和 User_Standard_Requests 实现。
  *          以 tmp\last_project 完成态为基准，B2 阶段裁剪：
  *          - 删除 GetConfigDescriptor/GetStringDescriptor (C1/C2)
  *          - 删除 Class_Data_Setup/Class_NoData_Setup (C4)
  *          - 删除 Class_Get_Interface_Setting (C3)
  *          - MASS_Reset 中 MASS_ConfigDescriptor[7] 改为硬编码 0xC0 (C1)
  *          - bDeviceState 赋值改为注释 (B3)
  ******************************************************************************
  */

#include "usb_lib.h"
#include "usb_desc.h"
#include "usb_pwr.h"

/* ── 端点配置表 ─────────────────────────────────────────────────────────── */
DEVICE Device_Table = {
    EP_NUM,   /* Total_Endpoint = 3 (EP0/EP1/EP2) */
    1         /* Total_Configuration = 1 */
};

/* ── 描述符包装 ──────────────────────────────────────────────────────────── */
static ONE_DESCRIPTOR Device_Descriptor = {
    (uint8_t *)MASS_DeviceDescriptor,
    MASS_SIZ_DEVICE_DESC
};

/* ── 前向声明 ────────────────────────────────────────────────────────────── */
static void MASS_init(void);
static void MASS_Reset(void);
static void MASS_Status_In(void);
static void MASS_Status_Out(void);
static uint8_t *MASS_GetDeviceDescriptor(uint16_t Length);

static void Mass_Storage_GetConfiguration(void);
static void Mass_Storage_SetConfiguration(void);
static void Mass_Storage_GetInterface(void);
static void Mass_Storage_SetInterface(void);
static void Mass_Storage_GetStatus(void);
static void Mass_Storage_ClearFeature(void);
static void Mass_Storage_SetEndPointFeature(void);
static void Mass_Storage_SetDeviceFeature(void);
static void Mass_Storage_SetDeviceAddress(void);

/* ── 设备属性回调表 ──────────────────────────────────────────────────────── */
DEVICE_PROP Device_Property = {
    MASS_init,                    /* Init */
    MASS_Reset,                   /* Reset */
    MASS_Status_In,               /* Process_Status_IN */
    MASS_Status_Out,              /* Process_Status_OUT */
    0,                            /* Class_Data_Setup — C4 实现 */
    0,                            /* Class_NoData_Setup — C4 实现 */
    0,                            /* Class_Get_Interface_Setting — C3 实现 */
    MASS_GetDeviceDescriptor,    /* GetDeviceDescriptor */
    0,                            /* GetConfigDescriptor — C1 实现 */
    0,                            /* GetStringDescriptor — C2 实现 */
    0,                            /* RxEP_buffer — 旧版兼容字段，未使用 */
    0x40                          /* MaxPacketSize — EP0 64 字节 */
};

/* ── 标准请求回调表 ──────────────────────────────────────────────────────── */
USER_STANDARD_REQUESTS User_Standard_Requests = {
    Mass_Storage_GetConfiguration,
    Mass_Storage_SetConfiguration,
    Mass_Storage_GetInterface,
    Mass_Storage_SetInterface,
    Mass_Storage_GetStatus,
    Mass_Storage_ClearFeature,
    Mass_Storage_SetEndPointFeature,
    Mass_Storage_SetDeviceFeature,
    Mass_Storage_SetDeviceAddress,
};

/* ═══════════════════════════════════════════════════════════════════════════
 *  Device_Property 回调实现
 * ═══════════════════════════════════════════════════════════════════════════ */

static void MASS_init(void)
{
    pInformation->Current_Configuration = 0;
    USB_SIL_Init();
    bDeviceState = UNCONNECTED;
}

static void MASS_Reset(void)
{
    Device_Info.Current_Configuration = 0;
    /* pInformation->Current_Feature = MASS_ConfigDescriptor[7]; */  /* C1 实现 */
    pInformation->Current_Feature = 0xC0;  /* B2 硬编码: 自供电 (§6.7 bmAttributes) */

    SetBTABLE(BTABLE_ADDRESS);

    /* EP0 — 控制端点 */
    SetEPType(ENDP0, EP_CONTROL);
    SetEPTxStatus(ENDP0, EP_TX_NAK);
    SetEPRxAddr(ENDP0, ENDP0_RXADDR);
    SetEPRxCount(ENDP0, Device_Property.MaxPacketSize);
    SetEPTxAddr(ENDP0, ENDP0_TXADDR);
    Clear_Status_Out(ENDP0);
    SetEPRxValid(ENDP0);

    /* EP1 — Bulk IN */
    SetEPType(ENDP1, EP_BULK);
    SetEPTxAddr(ENDP1, ENDP1_TXADDR);
    SetEPTxStatus(ENDP1, EP_TX_NAK);
    SetEPRxStatus(ENDP1, EP_RX_DIS);

    /* EP2 — Bulk OUT */
    SetEPType(ENDP2, EP_BULK);
    SetEPRxAddr(ENDP2, ENDP2_RXADDR);
    SetEPRxCount(ENDP2, Device_Property.MaxPacketSize);
    SetEPRxStatus(ENDP2, EP_RX_VALID);
    SetEPTxStatus(ENDP2, EP_TX_DIS);

    SetEPRxCount(ENDP0, Device_Property.MaxPacketSize);
    SetEPRxValid(ENDP0);
    SetDeviceAddress(0);

    bDeviceState = ATTACHED;
}

static void MASS_Status_In(void)
{
    /* EP0 IN 传输完成 — 核心库自动处理 */
}

static void MASS_Status_Out(void)
{
    /* EP0 OUT 传输完成 — 核心库自动处理 */
}

static uint8_t *MASS_GetDeviceDescriptor(uint16_t Length)
{
    return Standard_GetDescriptorData(Length, &Device_Descriptor);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  User_Standard_Requests 回调实现 (B2 空实现, C3 真实实现)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void Mass_Storage_GetConfiguration(void)    { /* 库自动处理 */ }
static void Mass_Storage_SetConfiguration(void)    { /* C3 实现状态机切换 */ }
static void Mass_Storage_GetInterface(void)        { /* 库自动处理 */ }
static void Mass_Storage_SetInterface(void)        { /* 单接口空操作 */ }
static void Mass_Storage_GetStatus(void)           { /* 库内部处理 */ }
static void Mass_Storage_ClearFeature(void)        { /* 无特殊处理 */ }
static void Mass_Storage_SetEndPointFeature(void)  { /* 无特殊处理 */ }
static void Mass_Storage_SetDeviceFeature(void)    { /* 无特殊处理 */ }
static void Mass_Storage_SetDeviceAddress(void)    { /* 库自动处理 */ }
