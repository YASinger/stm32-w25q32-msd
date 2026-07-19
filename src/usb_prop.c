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

/* ── MSC Bulk-Only 类请求码 (USB MSC 规范) ─────────────────────────────── */
#define GET_MAX_LUN         0xFE    /* 获取最大 LUN 号 (带 1 字节数据阶段, IN) */
#define MASS_STORAGE_RESET  0xFF    /* Bulk-Only 传输复位 (无数据阶段) */
#define LUN_DATA_LENGTH     0x01    /* GET_MAX_LUN 返回数据长度 */

/* 本设备仅 1 个 LUN (逻辑单元号 0), Max_Lun=0 表示仅 LUN 0 */
static uint32_t Max_Lun = 0;

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
static RESULT MASS_Get_Interface_Setting(uint8_t Interface, uint8_t AlternateSetting);
static RESULT MASS_Data_Setup(uint8_t RequestNo);
static RESULT MASS_NoData_Setup(uint8_t RequestNo);
static uint8_t *MASS_GetDeviceDescriptor(uint16_t Length);
static uint8_t *MASS_GetConfigDescriptor(uint16_t Length);
static uint8_t *MASS_GetStringDescriptor(uint16_t Length);

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
    MASS_Data_Setup,             /* Class_Data_Setup */
    MASS_NoData_Setup,           /* Class_NoData_Setup */
    MASS_Get_Interface_Setting,  /* Class_Get_Interface_Setting */
    MASS_GetDeviceDescriptor,    /* GetDeviceDescriptor */
    MASS_GetConfigDescriptor,    /* GetConfigDescriptor */
    MASS_GetStringDescriptor,   /* GetStringDescriptor */
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

static RESULT MASS_Get_Interface_Setting(uint8_t Interface, uint8_t AlternateSetting)
{
    if (Interface > 0) return USB_UNSUPPORT;
    return USB_SUCCESS;
}

/* ── Get_Max_Lun: 返回 1 字节 LUN 数 (Max_Lun=0 表示仅 LUN 0) ────────────── */
static uint8_t *Get_Max_Lun(uint16_t Length)
{
    if (Length == 0) {
        /* Length=0: 告知核心库数据阶段总长度 */
        pInformation->Ctrl_Info.Usb_wLength = LUN_DATA_LENGTH;
        return 0;
    }
    /* Length!=0: 返回数据指针 */
    return (uint8_t *)(&Max_Lun);
}

static RESULT MASS_Data_Setup(uint8_t RequestNo)
{
    uint8_t *(*CopyRoutine)(uint16_t) = NULL;

    /* GET_MAX_LUN: 类请求 + 接口接收, wValue=0, wIndex=0, wLength=1 */
    if ((Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT))
        && (RequestNo == GET_MAX_LUN)
        && (pInformation->USBwValue == 0)
        && (pInformation->USBwIndex == 0)
        && (pInformation->USBwLength == 0x01)) {
        CopyRoutine = Get_Max_Lun;
    } else {
        return USB_UNSUPPORT;
    }

    /* 登记数据阶段回调, 核心库据此完成 IN 数据传输 */
    pInformation->Ctrl_Info.CopyData = CopyRoutine;
    pInformation->Ctrl_Info.Usb_wOffset = 0;
    (*CopyRoutine)(0);             /* Length=0: 通知总数据长度 */

    return USB_SUCCESS;
}

static RESULT MASS_NoData_Setup(uint8_t RequestNo)
{
    /* MASS_STORAGE_RESET: 类请求 + 接口接收, wValue=0, wIndex=0, wLength=0 */
    if ((Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT))
        && (RequestNo == MASS_STORAGE_RESET)
        && (pInformation->USBwValue == 0)
        && (pInformation->USBwIndex == 0)
        && (pInformation->USBwLength == 0x00)) {

        /* 复位 Bulk 端点 DTOG, 恢复到 CBW 等待状态 (TR1 无 BOT 状态机, 仅清 DTOG) */
        ClearDTOG_TX(ENDP1);
        ClearDTOG_RX(ENDP2);

        return USB_SUCCESS;
    }

    return USB_UNSUPPORT;
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
 *  User_Standard_Requests 回调实现 (B2 空实现, C3 真实实现)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void Mass_Storage_GetConfiguration(void)    { /* 库自动处理 */ }
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
static void Mass_Storage_GetInterface(void)        { /* 库自动处理 */ }
static void Mass_Storage_SetInterface(void)        { /* 单接口空操作 */ }
static void Mass_Storage_GetStatus(void)           { /* 库内部处理 */ }
static void Mass_Storage_ClearFeature(void)        { /* 无特殊处理 */ }
static void Mass_Storage_SetEndPointFeature(void)  { /* 无特殊处理 */ }
static void Mass_Storage_SetDeviceFeature(void)    { /* 无特殊处理 */ }
static void Mass_Storage_SetDeviceAddress(void)    { /* 库自动处理 */ }
