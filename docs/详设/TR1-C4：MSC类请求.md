# TR1-C4 详细设计：MSC 类请求 GET_MAX_LUN / Bulk-Only Reset

## 修改记录

| 版本 | 日期 | 修改内容 | 修改人 |
|---|---|---|---|
| V1.0 | 2026-07-19 | 初始版本 | Copilot |

---

## 文档信息

| 项目 | 内容 |
|---|---|
| 需求编号 | TR1-C4 |
| 需求描述 | MSC 类请求 GET_MAX_LUN（返回 1 字节） / Bulk-Only Mass Storage Reset（清 DTOG） |
| 验收标准 | 设备管理器黄色感叹号消失（USBSTOR 驱动可加载） |
| 所属阶段 | TR1-C — 枚举完整化（横向补全） |
| 前置文档 | `项目框架设计.md`、`TR1框架设计.md` |
| 前置代码 | TR1-C3 标准请求完整响应已完成，`bDeviceState` 能到达 CONFIGURED |
| 完成态参照 | `tmp\last_project\src\usb_prop.c` |

---

## 1. 需求分解

TR1-C4 的目标是实现 MSC（Mass Storage Class）类请求，让 USBSTOR 驱动能成功加载。C1~C3 解决了标准 USB 枚举（设备/配置/字符串描述符 + SET_ADDRESS/SET_CONFIGURATION），但 USBSTOR 驱动初始化时还会发两个 MSC 类请求：

1. **GET_MAX_LUN**（0xFE）：查询设备支持的最大 LUN（逻辑单元号）。设备返回 1 字节，`Max_Lun=0` 表示仅 LUN 0。
2. **Bulk-Only Mass Storage Reset**（0xFF）：复位 BOT 传输状态机。设备清 EP1/EP2 的 DTOG。

**C4 解决的问题**：C1~C3 后 Problem Code 10（CM_PROB_FAILED_START）的根因。USBSTOR 驱动初始化时发 GET_MAX_LUN，若设备不响应（`Class_Data_Setup = NULL`），驱动加载失败。C4 实现这两个类请求后，USBSTOR 驱动成功加载，**黄色感叹号消失**。

本任务只修改一个文件：

| 操作 | 文件 | 内容 |
|---|---|---|
| 修改 | `src/usb_prop.c` | 补 `Max_Lun` 变量 + 类请求码宏 + `Get_Max_Lun`/`MASS_Data_Setup`/`MASS_NoData_Setup` 三个函数 + `Device_Property` 两个字段恢复 |

---

## 2. 关键技术决策

### 2.1 GET_MAX_LUN 请求格式（USB MSC 规范）

| 字段 | 值 | 说明 |
|---|---|---|
| bmRequestType | 0xA1 | 类请求 + 接口接收 + IN 方向 |
| bRequest | 0xFE | GET_MAX_LUN |
| wValue | 0x0000 | 保留 |
| wIndex | 0x0000 | 接口 0 |
| wLength | 0x0001 | 返回 1 字节 |

设备返回 1 字节 `Max_Lun`。`Max_Lun=0` 表示仅 1 个 LUN（LUN 0）。

**`Type_Recipient` 宏**（`usb_core.h`）：`pInformation->USBbmRequestType & (REQUEST_TYPE | RECIPIENT)`，用于判断请求类型和接收方。`CLASS_REQUEST | INTERFACE_RECIPIENT` = `0x20 | 0x01` = `0x21`。

### 2.2 Bulk-Only Mass Storage Reset 请求格式

| 字段 | 值 | 说明 |
|---|---|---|
| bmRequestType | 0x21 | 类请求 + 接口接收 + OUT 方向 |
| bRequest | 0xFF | MASS_STORAGE_RESET |
| wValue | 0x0000 | 保留 |
| wIndex | 0x0000 | 接口 0 |
| wLength | 0x0000 | 无数据阶段 |

设备收到后清 EP1/EP2 的 DTOG，恢复到 CBW 等待状态。TR1 阶段无 BOT 状态机（TR2 才做），只清 DTOG。

### 2.3 `Get_Max_Lun` 的两阶段回调机制

`Get_Max_Lun(uint16_t Length)` 被 `MASS_Data_Setup` 登记为数据阶段回调，被 USB 库的 `usb_core.c` 多次调用：

| Length 值 | 调用目的 | 返回值 |
|---|---|---|
| 0 | 通知核心库数据阶段总长度 | NULL（但设置 `Usb_wLength = 1`） |
| 1 | 获取第 1 字节数据 | `&Max_Lun`（指向 `Max_Lun` 的指针） |

这是 USB 库的标准数据阶段回调协议——`Length=0` 时报长度，`Length>0` 时返回数据指针。

### 2.4 `Max_Lun` 变量

```c
static uint32_t Max_Lun = 0;
```

本设备仅 1 个 LUN（LUN 0），`Max_Lun=0`。`uint32_t` 类型是完成态的选择（虽然返回 1 字节，但用 32 位存储避免对齐问题）。

### 2.5 `Device_Property` 两个字段恢复

| 字段 | B2 值 | C4 值 |
|---|---|---|
| `Class_Data_Setup` | 0 (NULL) | `MASS_Data_Setup` |
| `Class_NoData_Setup` | 0 (NULL) | `MASS_NoData_Setup` |

---

## 3. 涉及组件

```
┌─────────────────────────────────────────────────┐
│  USBSTOR 驱动初始化                              │
│  ├─ GET_MAX_LUN (0xFE)  → 设备返回 Max_Lun=0   │
│  └─ BOT Reset (0xFF)    → 设备清 EP1/EP2 DTOG  │
└────────────────────┬────────────────────────────┘
                     │ EP0 控制传输
╔════════════════════╧═════════════════════════════╗
║  device (USB 设备层)                              ║
║  ┌───────────────────────────────────────────┐  ║
║  │ usb_prop.c                                │  ║
║  │  ├─ Max_Lun = 0              LUN 数变量   │  ║
║  │  ├─ Get_Max_Lun()            数据阶段回调 │  ║
║  │  ├─ MASS_Data_Setup()        类请求分发   │  ║
║  │  ├─ MASS_NoData_Setup()      类请求分发   │  ║
║  │  └─ Device_Property:                       │  ║
║  │     ├─ Class_Data_Setup → MASS_Data_Setup │  ║
║  │     └─ Class_NoData_Setup → MASS_NoData_Setup│
║  └───────────────────────────────────────────┘  ║
╚══════════════════════════════════════════════════╝
```

---

## 4. 文件清单

### 4.1 修改文件

| 文件 | 修改内容 |
|---|---|
| `src/usb_prop.c` | 补 `Max_Lun` 变量 + 类请求码宏 + 3 个函数实现 + `Device_Property` 两个字段恢复 |

### 4.2 新建文件

无。

### 4.3 工程分组

无变化。

---

## 5. 接口设计

### 5.1 `src/usb_prop.c` 修改

**改动 1**：补类请求码宏和 `Max_Lun` 变量（在 `Device_Table` 前）：

```c
/* ── MSC Bulk-Only 类请求码 (USB MSC 规范) ─────────────────────────────── */
#define GET_MAX_LUN         0xFE    /* 获取最大 LUN 号 (带 1 字节数据阶段, IN) */
#define MASS_STORAGE_RESET  0xFF    /* Bulk-Only 传输复位 (无数据阶段) */
#define LUN_DATA_LENGTH     0x01    /* GET_MAX_LUN 返回数据长度 */

/* 本设备仅 1 个 LUN (逻辑单元号 0), Max_Lun=0 表示仅 LUN 0 */
static uint32_t Max_Lun = 0;
```

**改动 2**：补前向声明：

```c
static RESULT MASS_Data_Setup(uint8_t RequestNo);
static RESULT MASS_NoData_Setup(uint8_t RequestNo);
```

**改动 3**：`Device_Property` 两个字段恢复：

```c
/* 改前 */
0,                            /* Class_Data_Setup — C4 实现 */
0,                            /* Class_NoData_Setup — C4 实现 */
/* 改后 */
MASS_Data_Setup,             /* Class_Data_Setup */
MASS_NoData_Setup,           /* Class_NoData_Setup */
```

**改动 4**：补 `Get_Max_Lun`/`MASS_Data_Setup`/`MASS_NoData_Setup` 实现（在 `MASS_Get_Interface_Setting` 后、`MASS_GetDeviceDescriptor` 前）：

```c
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
```

---

## 6. 实现要点与风险

### 6.1 C4 的可观测变化（Problem Code 10 消失）

**C3 → C4 的 USBTreeView 变化**：

| 项 | C3 | C4 | 变化 |
|---|---|---|---|
| Problem Code | 10 | **消失** | ✅ 黄色感叹号消失 |
| Driver | USBSTOR.SYS（启动失败） | **USBSTOR.SYS（正常）** | ✅ 驱动加载成功 |
| Used Endpoints | 1 | **3** | ✅ EP1+EP2 激活 |
| String Descriptors 详细 | not available | **完整可读** | ✅ Problem Code 消失后可读 |
| Current Config Value | 0x00 | **0x01** | ✅ SET_CONFIGURATION 成功 |

**C4 是 TR1 阶段最后的大变化**——Problem Code 10 消失，设备完全正常工作。

### 6.2 `Type_Recipient` 宏的依赖

`Type_Recipient` 定义在 `usb_core.h`：
```c
#define Type_Recipient (pInformation->USBbmRequestType & (REQUEST_TYPE | RECIPIENT))
```

`pInformation` 是 USB 库的全局指针（`usb_init.c` 定义），在 `USB_Init()` 后有效。C4 的类请求在枚举完成后（CONFIGURED 状态）由 USBSTOR 驱动发起，`pInformation` 已初始化，无风险。

### 6.3 `Get_Max_Lun` 的 `Length=0` 协议

`Length=0` 时 `Get_Max_Lun` 设置 `Usb_wLength = 1` 并返回 0（NULL）。USB 库的 `usb_core.c` 据此知道数据阶段总长度为 1 字节，后续用 `Length=1` 再次调用获取数据指针。

**注意**：返回 NULL 不会导致 HardFault——USB 库在 `Length=0` 时不解引用返回值。

### 6.4 BOT Reset 的 DTOG 清除

`ClearDTOG_TX(ENDP1)` / `ClearDTOG_RX(ENDP2)` 与 C3 的 `Mass_Storage_SetConfiguration()` 中的调用相同。TR1 阶段 EP1/EP2 不实际传输数据（回调是 `NOP_Process`），但 DTOG 清除仍需执行——USBSTOR 驱动期望 BOT Reset 后端点状态复位。

---

## 7. 验证流程

### 7.1 编译验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | Keil Build (F7) | 0 Error(s), 0 Warning(s) | 待验证 |

### 7.2 USBTreeView 验证（核心验收）

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 烧录，插 USB | 设备管理器无黄色感叹号 | **C4 核心验收** |
| 2 | Problem Code | 无（0） | **C4 核心验收** |
| 3 | Used Endpoints | 3（EP0+EP1+EP2） | 待验证 |
| 4 | String Descriptors 详细 | 5 组完整可读 | 待验证 |
| 5 | Current Config Value | 0x01 | 待验证 |

### 7.3 对照完成态

C4 完成后，USBTreeView 日志应与 `usb tree 0718 0015.txt`（完成态）基本一致：
- Connection Status = 0x01 (Device is connected)
- Problem Code = 无
- 设备描述符 + 配置描述符 + 字符串描述符 全部完整
- USBSTOR.SYS + disk.sys 驱动加载

---

## 8. 常见问题排查

### 8.1 Problem Code 10 仍存在

**原因**：`Device_Property.Class_Data_Setup` 仍为 NULL，或 `MASS_Data_Setup` 未正确响应 GET_MAX_LUN。

**排查**：
- 确认 `Device_Property` 的 `Class_Data_Setup` 字段已改为 `MASS_Data_Setup`（§5.1 改动 3）
- 确认 `MASS_Data_Setup` 的 GET_MAX_LUN 判断条件正确（`wLength == 0x01`）

### 8.2 编译报错：identifier "CLASS_REQUEST" is undefined

**原因**：`CLASS_REQUEST` / `INTERFACE_RECIPIENT` 来自 `usb_def.h`，经 `usb_lib.h` 间接包含。

**修复**：确认 `usb_prop.c` 第一行有 `#include "usb_lib.h"`（B2 已有）。

### 8.3 GET_MAX_LUN 返回错误数据

**原因**：`Max_Lun` 值错误，或 `Get_Max_Lun` 的 `Length=0` 分支未设置 `Usb_wLength`。

**排查**：确认 `Max_Lun = 0`，`Get_Max_Lun` 的 `Length==0` 分支设置 `Usb_wLength = LUN_DATA_LENGTH`（1）。

---

## 9. 与下一步的衔接

TR1-C4 完成后，USBSTOR 驱动成功加载，黄色感叹号消失。设备在 PC 端完全正常识别。下一步：

- **TR1-C5**：`USB_Cable_Config(DISABLE/ENABLE)` 软件重连。调用函数后 PC 重新枚举。这是 TR1 阶段的最后一步。

> **C4 是 TR1 阶段的收官之战**：C1~C3 逐步补全描述符和标准请求，C4 补全类请求后 USBSTOR 驱动加载成功。C4 完成后，设备在 PC 端的表现与最终产品一致（除了无法读写存储——那是 TR2 的事）。

---

*文档结束。*
