/**
  ******************************************************************************
  * @file    usb_prop.c
  * @brief   USB 设备属性 — Device_Property 和 User_Standard_Requests 实现
  ******************************************************************************
  */

#include "usb_lib.h"
#include "usb_desc.h"
#include "usb_pwr.h"
#include "usb_bot.h"
#include "mass_mal.h"

/* ── MSC Bulk-Only 类请求码 (USB MSC 规范) ─────────────────────────────── */
#define GET_MAX_LUN         0xFE    /* 获取最大 LUN 号 (带 1 字节数据阶段, IN) */
#define MASS_STORAGE_RESET  0xFF    /* Bulk-Only 传输复位 (无数据阶段) */
#define LUN_DATA_LENGTH     0x01    /* GET_MAX_LUN 返回数据长度 */

/* 本设备仅 1 个 LUN (逻辑单元号 0), Max_Lun=0 表示仅 LUN 0 */
uint32_t Max_Lun = 0;

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

/* ── 设备属性回调表 ────────────────────────────────────────────────────────
 * 类型 DEVICE_PROP (usb_core.h)，由 USB_Init() 登记为全局 pProperty。
 * 触发者：核心库在 EP0 控制传输的各阶段调用，负责类请求分发与描述符返回。
 * ────────────────────────────────────────────────────────────────────────── */
DEVICE_PROP Device_Property = {
    MASS_init,                    /* Init       —— USB_Init() 时调用，初始化设备状态与 USB 外设 */
    MASS_Reset,                   /* Reset      —— USB 复位中断时调用，配置 EP0/EP1/EP2 类型与收发地址 */
    MASS_Status_In,               /* Process_Status_IN  —— EP0 状态阶段(零长 IN) 完成后调用，本项目空 */
    MASS_Status_Out,              /* Process_Status_OUT —— EP0 状态阶段(零长 OUT)完成后调用，本项目空 */
    MASS_Data_Setup,              /* Class_Data_Setup   —— EP0 收到带数据阶段的类请求时调用(如 GET MAX LUN) */
    MASS_NoData_Setup,            /* Class_NoData_Setup —— EP0 收到不带数据的类请求时调用(如 Bulk-Only Reset) */
    MASS_Get_Interface_Setting,   /* Class_Get_Interface_Setting —— 库校验 SET_INTERFACE 时调用，返回是否支持 */
    MASS_GetDeviceDescriptor,    /* GetDeviceDescriptor —— 主机请求设备描述符(GET_DESCRIPTOR)时调用 */
    MASS_GetConfigDescriptor,    /* GetConfigDescriptor —— 主机请求配置描述符时调用 */
    MASS_GetStringDescriptor,     /* GetStringDescriptor —— 主机请求字符串描述符时调用 */
    0,                            /* RxEP_buffer —— 旧版兼容字段，当前库版本未使用 */
    0x40                          /* MaxPacketSize —— EP0 最大包长 64 字节(Full-Speed) */
};

/* ── 标准请求回调表 ────────────────────────────────────────────────────────
 * 类型 USER_STANDARD_REQUESTS (usb_core.h)，由 USB_Init() 登记为全局 pUser_Standard_Requests。
 * 触发者：核心库在标准请求(SET_ADDRESS/SET_CONFIGURATION 等)协议层处理完后调用，
 *         作为用户层后处理钩子，用于状态机切换、端点 DTOG 清除等。
 * ────────────────────────────────────────────────────────────────────────── */
USER_STANDARD_REQUESTS User_Standard_Requests = {
    Mass_Storage_GetConfiguration,    /* User_GetConfiguration   —— GET_CONFIGURATION 处理完后调用，本项目空(库已返回当前配置) */
    Mass_Storage_SetConfiguration,   /* User_SetConfiguration   —— SET_CONFIGURATION 处理完后调用，切换 bDeviceState 并清 EP1/EP2 DTOG */
    Mass_Storage_GetInterface,        /* User_GetInterface       —— GET_INTERFACE 处理完后调用，本项目空(库已返回当前 alt) */
    Mass_Storage_SetInterface,        /* User_SetInterface       —— SET_INTERFACE 处理完后调用，本项目单接口空操作 */
    Mass_Storage_GetStatus,          /* User_GetStatus          —— GET_STATUS 处理完后调用，本项目空(库内按 Type_Recipient 处理) */
    Mass_Storage_ClearFeature,       /* User_ClearFeature       —— CLEAR_FEATURE 处理完后调用，本项目无特殊处理 */
    Mass_Storage_SetEndPointFeature, /* User_SetEndPointFeature —— SET_FEATURE(端点) 处理完后调用，本项目无特殊处理 */
    Mass_Storage_SetDeviceFeature,   /* User_SetDeviceFeature   —— SET_FEATURE(设备) 处理完后调用，本项目无特殊处理 */
    Mass_Storage_SetDeviceAddress,   /* User_SetDeviceAddress   —— SET_ADDRESS 处理完后调用，本项目空(USB 核心库自动处理地址) */
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

    /* TR2-S1: 初始化 BOT 状态机 */
    Bot_State = BOT_IDLE;
    CBW.dSignature = BOT_CBW_SIGNATURE;

    /* TR2-S2: 初始化 SRAM 介质层 */
    MAL_Init(0);

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

        /* 复位 Bulk 端点 DTOG, 恢复到 CBW 等待状态 */
        ClearDTOG_TX(ENDP1);
        ClearDTOG_RX(ENDP2);

        /* TR2-S1: 重置 BOT 状态机 */
        Bot_State = BOT_IDLE;
        CBW.dSignature = BOT_CBW_SIGNATURE;

        return USB_SUCCESS;
    }

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
        /* TR2-S1: 配置成功后重置 BOT 状态机 */
        Bot_State = BOT_IDLE;
        CBW.dSignature = BOT_CBW_SIGNATURE;
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
