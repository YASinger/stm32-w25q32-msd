/**
  ******************************************************************************
  * @file    usb_prop.c
  * @brief   USB 设备属性 — Device_Property 和 User_Standard_Requests 实现
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

static ONE_DESCRIPTOR Config_Descriptor = {
    (uint8_t *)MASS_ConfigDescriptor,
    MASS_SIZ_CONFIG_DESC
};

static ONE_DESCRIPTOR String_Descriptor[5] = {
    {(uint8_t *)MASS_StringLangID,    MASS_SIZ_STRING_LANGID},
    {(uint8_t *)MASS_StringVendor,    MASS_SIZ_STRING_VENDOR},
    {(uint8_t *)MASS_StringProduct,   MASS_SIZ_STRING_PRODUCT},
    {(uint8_t *)MASS_StringSerial,    MASS_SIZ_STRING_SERIAL},
    {(uint8_t *)MASS_StringInterface, MASS_SIZ_STRING_INTERFACE},
};

/* ── 前向声明 ────────────────────────────────────────────────────────────── */
static void MASS_init(void);
static void MASS_Reset(void);
static void MASS_Status_In(void);
static void MASS_Status_Out(void);
static RESULT MASS_Data_Setup(uint8_t RequestNo);
static RESULT MASS_NoData_Setup(uint8_t RequestNo);
static uint8_t *MASS_GetDeviceDescriptor(uint16_t Length);
static uint8_t *MASS_GetConfigDescriptor(uint16_t Length);
static uint8_t *MASS_GetStringDescriptor(uint16_t Length);
static RESULT MASS_Get_Interface_Setting(uint8_t Interface, uint8_t AlternateSetting);

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
    MASS_init,
    MASS_Reset,
    MASS_Status_In,
    MASS_Status_Out,
    MASS_Data_Setup,
    MASS_NoData_Setup,
    MASS_Get_Interface_Setting,
    MASS_GetDeviceDescriptor,
    MASS_GetConfigDescriptor,
    MASS_GetStringDescriptor,
    0,                        /* RxEP_buffer */
    0x40                      /* MaxPacketSize = 64 */
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
    pInformation->Current_Feature = MASS_ConfigDescriptor[7];

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

static RESULT MASS_Data_Setup(uint8_t RequestNo)
{
    /* MSC 类请求 (有数据阶段) — TR1 暂不处理, 返回 STALL */
    return USB_UNSUPPORT;
}

static RESULT MASS_NoData_Setup(uint8_t RequestNo)
{
    /* MSC 类请求 (无数据阶段) — TR1 暂不处理, 返回 STALL */
    return USB_UNSUPPORT;
}

static RESULT MASS_Get_Interface_Setting(uint8_t Interface, uint8_t AlternateSetting)
{
    if (Interface > 0) return USB_UNSUPPORT;
    return USB_SUCCESS;
}

static uint8_t *MASS_GetDeviceDescriptor(uint16_t Length)
{
    return Standard_GetDescriptorData(Length, &Device_Descriptor);
}

static uint8_t *MASS_GetConfigDescriptor(uint16_t Length)
{
    return Standard_GetDescriptorData(Length, &Config_Descriptor);
}

static uint8_t *MASS_GetStringDescriptor(uint16_t Length)
{
    uint8_t index = pInformation->USBwValue0;
    uint8_t *pBuf = NULL;

    if (index < 5) {
        pBuf = Standard_GetDescriptorData(Length, &String_Descriptor[index]);
    }

    return pBuf;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  User_Standard_Requests 回调实现
 * ═══════════════════════════════════════════════════════════════════════════ */

static void Mass_Storage_GetConfiguration(void)
{
    /* 库在调用后返回 &pInformation->Current_Configuration */
}

static void Mass_Storage_SetConfiguration(void)
{
    if (pInformation->Current_Configuration != 0) {
        bDeviceState = CONFIGURED;
        ClearDTOG_TX(ENDP1);
        ClearDTOG_RX(ENDP2);
    } else {
        bDeviceState = ADDRESSED;
    }
}

static void Mass_Storage_GetInterface(void)
{
    /* 库在调用后返回 &pInformation->Current_AlternateSetting */
}

static void Mass_Storage_SetInterface(void)
{
    /* 单一接口，空操作 */
}

static void Mass_Storage_GetStatus(void)
{
    /* 库的 Standard_GetStatus() 内部处理 Type_Recipient */
}

static void Mass_Storage_ClearFeature(void)
{
    /* 无特殊处理 */
}

static void Mass_Storage_SetEndPointFeature(void)
{
    /* 无特殊处理 */
}

static void Mass_Storage_SetDeviceFeature(void)
{
    /* 无特殊处理 */
}

static void Mass_Storage_SetDeviceAddress(void)
{
    /* USB 核心库自动处理 */
}
