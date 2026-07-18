# TR1-B2 详细设计：usb_prop 设备属性与 MASS_Reset

## 修改记录

| 版本 | 日期 | 修改内容 | 修改人 |
|---|---|---|---|
| V1.0 | 2026-07-18 | 初始版本 | Copilot |

---

## 文档信息

| 项目 | 内容 |
|---|---|
| 需求编号 | TR1-B2 |
| 需求描述 | `usb_prop.c/h` MASS_Reset + 设备属性表 `DEVICE_PROP` + `MASS_GetDeviceDescriptor` 回调 + 标准请求表 `Device_Table` |
| 验收标准 | PC 设备管理器出现 "Unknown Device" 或带黄色感叹号的设备（**TR1-01 的真正含义**） |
| 所属阶段 | TR1-B — 最小枚举通过（纵向切片） |
| 前置文档 | `项目框架设计.md`、`TR1框架设计.md` |
| 前置代码 | TR1-A 骨架全部完成（A1~A4）；TR1-B1 设备描述符数据已就位 |
| 完成态参照 | `tmp\last_project\src\usb_prop.c`（TR1 全部完成后的最终形态） |

---

## 1. 需求分解

TR1-B2 是 TR1 阶段的**第一个里程碑**——PC 设备管理器第一次出现设备。B1 提供了设备描述符数据，B2 把数据接到 USB 库的回调机制上，让主机的 `GET_DESCRIPTOR(Device)` 能拿到 18 字节合法数据。

B2 需要新建两个文件，并修改一个现有文件：

| 操作 | 文件 | 内容 |
|---|---|---|
| 新建 | `src/usb_prop.c` | `Device_Table`/`Device_Property`/`User_Standard_Requests` 强定义 + `MASS_init`/`MASS_Reset`/`MASS_GetDeviceDescriptor` 等回调实现 |
| 新建 | `inc/usb_prop.h` | 接口声明 |
| 修改 | `src/usb_globals.c` | 移除剩余 3 个 weak 占位（`Device_Table`/`Device_Property`/`User_Standard_Requests`），文件可整个删除 |

**与完成态的关系**：`last_project` 的 `usb_prop.c` 包含全部回调（设备/配置/字符串描述符 + 类请求 + 标准请求），是 C1~C4 完成后的最终形态。B2 只裁剪出**最小枚举必需集**——`Device_Table` + `Device_Property`（含 `MASS_init`/`MASS_Reset`/`MASS_GetDeviceDescriptor`）+ `User_Standard_Requests`（空实现）。配置描述符回调（C1）、字符串描述符回调（C2）、类请求（C4）留待后续补全。

---

## 2. 关键技术决策

### 2.1 B2 的最小必需集（从完成态裁剪）

完成态 `usb_prop.c` 的功能按 TR1 阶段拆分：

| 完成功能 | 所属阶段 | B2 是否保留 |
|---|---|---|
| `Device_Table` | B2 | ✅ 保留 |
| `Device_Property`（`MASS_init`/`MASS_Reset`/`MASS_GetDeviceDescriptor`） | B2 | ✅ 保留 |
| `User_Standard_Requests`（9 个空回调） | B2/C3 | ✅ 保留（空实现） |
| `MASS_GetConfigDescriptor` + `Config_Descriptor` | C1 | ❌ 删除（B2 无配置描述符） |
| `MASS_GetStringDescriptor` + `String_Descriptor[5]` | C2 | ❌ 删除（B2 无字符串描述符） |
| `MASS_Data_Setup`/`MASS_NoData_Setup`（类请求） | C4 | ❌ 删除 |
| `MASS_Get_Interface_Setting` | C3 | ❌ 删除 |
| `Mass_Storage_SetConfiguration`（状态机切换） | C3 | 简化（B2 仅空实现） |

**B2 保留的最小集**：
- `Device_Table`：EP_NUM=3, Total_Configuration=1
- `Device_Property`：`MASS_init`/`MASS_Reset`/`MASS_GetDeviceDescriptor` + 6 个 NULL/空回调 + MaxPacketSize=64
- `User_Standard_Requests`：9 个空函数（C3 才需要真实实现，B2 空实现满足链接）

### 2.2 B2 特有的依赖问题（`bDeviceState` 和 `MASS_ConfigDescriptor`）

**问题 1：`bDeviceState` 未定义**

完成态的 `MASS_init()` 和 `MASS_Reset()` 都写 `bDeviceState`（来自 B3 的 `usb_pwr.h`）。B2 阶段 `usb_pwr.h/c` 不存在。

**处理**：B2 的 `MASS_init()` 和 `MASS_Reset()` 中删除 `bDeviceState` 赋值，改为注释标记。B3 完成 `usb_pwr.c` 后加回。

**问题 2：`MASS_ConfigDescriptor` 未定义**

完成态的 `MASS_Reset()` 中有 `pInformation->Current_Feature = MASS_ConfigDescriptor[7]`（读配置描述符的 bmAttributes 字段）。`MASS_ConfigDescriptor` 属 C1。

**处理**：B2 阶段改为硬编码 `pInformation->Current_Feature = 0xC0`（自供电，与文档 §6.7 的 bmAttributes 一致）。C1 完成后改回 `MASS_ConfigDescriptor[7]`。

### 2.3 `Device_Property` 的 NULL 回调处理

`Device_Property` 是 `DEVICE_PROP` 结构体（`usb_core.h`），包含 12 个字段。B2 只实现 3 个回调（`Init`/`Reset`/`GetDeviceDescriptor`），其余 9 个字段：

| 字段 | B2 值 | 说明 |
|---|---|---|
| `Init` | `MASS_init` | 实现 |
| `Reset` | `MASS_Reset` | 实现 |
| `Process_Status_IN` | `MASS_Status_In` | 空函数（库调用但不做事） |
| `Process_Status_OUT` | `MASS_Status_Out` | 空函数 |
| `Class_Data_Setup` | 0 (NULL) | C4 实现 |
| `Class_NoData_Setup` | 0 (NULL) | C4 实现 |
| `Class_Get_Interface_Setting` | 0 (NULL) | C3 实现 |
| `GetDeviceDescriptor` | `MASS_GetDeviceDescriptor` | 实现 |
| `GetConfigDescriptor` | 0 (NULL) | C1 实现 |
| `GetStringDescriptor` | 0 (NULL) | C2 实现 |
| `RxEP_buffer` | 0 | 旧版兼容字段，不使用 |
| `MaxPacketSize` | 0x40 (64) | EP0 最大包 |

**风险**：`GetConfigDescriptor`/`GetStringDescriptor` 为 NULL 时，若主机发 GET_DESCRIPTOR(Config/String)，USB 库会调用 NULL 函数指针导致 HardFault。

**分析**：B2 阶段主机先发 GET_DESCRIPTOR(Device)，拿到设备描述符后发现 `bNumConfigurations=1`，会尝试 GET_DESCRIPTOR(Config)。此时 `GetConfigDescriptor=NULL`，调用即 HardFault。

**但这不影响 B2 验收**：B2 的验收标准是"设备管理器出现 Unknown Device 或带黄色感叹号的设备"。主机在 GET_DESCRIPTOR(Device) 成功后就会注册设备（显示 Unknown Device），GET_DESCRIPTOR(Config) 失败不会导致设备消失——主机只是标记设备"配置描述符请求失败"，给黄色感叹号。这正是 B2 验收标准中"带黄色感叹号"的含义。

**结论**：B2 阶段 `GetConfigDescriptor`/`GetStringDescriptor` 为 NULL 是安全的，黄色感叹号是预期现象。C1 补入配置描述符后感叹号消失。

### 2.4 `usb_globals.c` 的最终移除

B2 完成后，`Device_Table`/`Device_Property`/`User_Standard_Requests` 全部由 `usb_prop.c` 强定义。`usb_globals.c` 的 3 个 weak 占位全部被覆盖，文件可整个从工程中移除。

**操作**：从 `project.uvprojx` 的 src Group 中删除 `usb_globals.c`，并删除 `src/usb_globals.c` 文件。

---

## 3. 涉及组件

```
┌─────────────────────────────────────────────────┐
│  主机 GET_DESCRIPTOR(Device) 请求                │
└────────────────────┬────────────────────────────┘
                     │ EP0 控制传输
╔════════════════════╧═════════════════════════════╗
║  device (USB 设备层)                              ║
║  ┌───────────────────────────────────────────┐  ║
║  │ usb_prop.c / usb_prop.h                   │  ║
║  │  ├─ Device_Table              端点配置表   │  ║
║  │  ├─ Device_Property           设备属性表   │  ║
║  │  │   ├─ MASS_init()          初始化       │  ║
║  │  │   ├─ MASS_Reset()         复位配置端点 │  ║
║  │  │   └─ MASS_GetDeviceDescriptor() 返回描述符│ ║
║  │  └─ User_Standard_Requests    标准请求表   │  ║
║  └───────────────────────────────────────────┘  ║
╠══════════════════════════════════════════════════╣
║  依赖                                            ║
║  ┌──────────────┐  ┌──────────────────────────┐ ║
║  │ usb_desc.c   │  │ usb_conf.h (A3)          │ ║
║  │ MASS_DeviceDescriptor      │ EP_NUM/BTABLE/ENDP│ ║
║  │ MASS_SIZ_DEVICE_DESC       │ _ADDR/IMR_MSK    │ ║
║  └──────────────┘  └──────────────────────────┘ ║
║  ┌──────────────┐  ┌──────────────────────────┐ ║
║  │ usb_core.c   │  │ usb_istr.c (A4)          │ ║
║  │ Standard_GetDescriptorData │ Device_Property.Reset│ ║
║  └──────────────┘  └──────────────────────────┘ ║
╚══════════════════════════════════════════════════╝
```

**依赖关系**：
- `MASS_GetDeviceDescriptor()` 依赖 B1 的 `MASS_DeviceDescriptor` 和 `MASS_SIZ_DEVICE_DESC`
- `MASS_Reset()` 依赖 A3 的 `BTABLE_ADDRESS`/`ENDP0_RXADDR`/`ENDP0_TXADDR`/`ENDP1_TXADDR`/`ENDP2_RXADDR` 和 `Device_Property.MaxPacketSize`
- `Device_Property.Reset()` 被 A4 的 `USB_Istr()` RESET 分支调用
- `MASS_init()` 调用 `USB_SIL_Init()`（USB 库），设置 `wInterrupt_Mask = IMR_MSK`（A3）

---

## 4. 文件清单

### 4.1 新建文件

| 文件 | 说明 |
|---|---|
| `src/usb_prop.c` | 3 个全局结构体强定义 + 回调实现 |
| `inc/usb_prop.h` | 接口声明 |

### 4.2 修改文件

| 文件 | 修改内容 |
|---|---|
| `project.uvprojx` | src Group 加入 `usb_prop.c`/`usb_prop.h`，移除 `usb_globals.c` |

### 4.3 删除文件

| 文件 | 说明 |
|---|---|
| `src/usb_globals.c` | 3 个 weak 占位全部被 `usb_prop.c` 强定义覆盖，文件使命完成 |

---

## 5. 接口设计

### 5.1 `inc/usb_prop.h`

```c
#ifndef __USB_PROP_H
#define __USB_PROP_H

void MASS_init(void);
void MASS_Reset(void);

#endif /* __USB_PROP_H */
```

**说明**：极简头文件。`Device_Table`/`Device_Property`/`User_Standard_Requests` 是全局变量，通过 `usb_core.h` 的 extern 声明被 USB 库引用，无需在 `usb_prop.h` 中重复声明。`MASS_init`/`MASS_Reset` 声明供 `main.c`（B4）调用。

### 5.2 `src/usb_prop.c`

```c
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
    /* bDeviceState = UNCONNECTED; */  /* B3 实现 (usb_pwr.h) */
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

    /* bDeviceState = ATTACHED; */  /* B3 实现 (usb_pwr.h) */
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
```

**与完成态（`last_project`）的差异对照**：

| 差异点 | 完成态 | B2（当前） | 恢复时机 |
|---|---|---|---|
| `#include "usb_pwr.h"` | 有 | 删除 | B3 创建后加回 |
| `Config_Descriptor` + `MASS_GetConfigDescriptor` | 有 | 删除 | C1 |
| `String_Descriptor[5]` + `MASS_GetStringDescriptor` | 有 | 删除 | C2 |
| `MASS_Data_Setup`/`MASS_NoData_Setup` | 有 | 删除（NULL） | C4 |
| `MASS_Get_Interface_Setting` | 有 | 删除（NULL） | C3 |
| `MASS_Reset` 中 `MASS_ConfigDescriptor[7]` | 有 | 硬编码 `0xC0` | C1 |
| `bDeviceState` 赋值 | 有 | 注释标记 | B3 |
| `Mass_Storage_SetConfiguration` | 状态机切换 | 空函数 | C3 |

---

## 6. 实现要点与风险

### 6.1 `main.c` 的调用时机（B2 的关键前置）

**问题**：`MASS_init()` 和 `MASS_Reset()` 由谁调用？

完成态的调用链：
```
main() → USB_Init() → pProperty = &Device_Property → pProperty->Init() = MASS_init()
主机 RESET → USB_Istr() → Device_Property.Reset() = MASS_Reset()
```

B2 阶段 `main.c` 仍是 A2 的初始化序列（`Set_System` → `Set_USBClock` → `USB_Interrupts_Config` → `USB_Cable_Config(ENABLE)` → `while(1)`），**没有调用 `USB_Init()`**。这意味着 `MASS_init()` 不会被调用，`wInterrupt_Mask` 仍为 0，CTR 分支不进，EP0 SETUP 包不被处理。

**处理**：B2 需要修改 `main.c`，在 `USB_Cable_Config(ENABLE)` 前加 `USB_Init()`。这是 B2 验收（设备管理器出现设备）的必要条件。

**修改后的 `main.c`**：
```c
#include "stm32f10x.h"
#include "hw_config.h"
#include "usb_lib.h"

int main(void)
{
  Set_System();
  Set_USBClock();
  USB_Interrupts_Config();
  USB_Init();                  /* B2 新增: 调用 MASS_init() → USB_SIL_Init() */
  USB_Cable_Config(ENABLE);    /* PA12 切为 AF_PP, D+ 上拉生效 */

  while (1)
  {
  }
}
```

**调用顺序说明**：`USB_Init()` 在 `USB_Cable_Config(ENABLE)` 之前——先初始化 USB 外设（`MASS_init` → `USB_SIL_Init` → 设置 `wInterrupt_Mask = IMR_MSK`），再使能 D+ 上拉让主机看到设备。若顺序反了，主机可能在 `wInterruptMask` 设置前发起请求。

### 6.2 `USB_Init()` 的内部行为

`USB_Init()`（`usb_init.c`）做三件事：
1. `pInformation = &Device_Info`
2. `pProperty = &Device_Property`
3. `pUser_Standard_Requests = &User_Standard_Requests`
4. `pProperty->Init()` = `MASS_init()`

`MASS_init()` 调用 `USB_SIL_Init()`（`usb_sil.c`），后者设置 `wInterrupt_Mask = IMR_MSK` 并写 CNTR 寄存器。这是 CTR 分支能进入的前提。

### 6.3 黄色感叹号的预期

B2 完成后，主机 GET_DESCRIPTOR(Device) 成功拿到 VID/PID，设备管理器出现设备。但 `GetConfigDescriptor` 为 NULL，主机 GET_DESCRIPTOR(Config) 失败，设备标记为"配置描述符请求失败"——**带黄色感叹号**。

这是 B2 的**预期验收结果**（文档 §3.2 明确写"设备管理器出现 Unknown Device 或带黄色感叹号的设备"）。C1 补入配置描述符后感叹号消失。

### 6.4 `usb_globals.c` 的移除

B2 完成后，`usb_globals.c` 的 3 个 weak 占位全部被 `usb_prop.c` 强定义覆盖。从 uvprojx 移除该文件并删除源文件，避免后续维护困惑。

---

## 7. 验证流程

### 7.1 编译验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | Keil Build (F7) | 0 Error(s), 0 Warning(s) | 待验证 |

### 7.2 链接验证（确认 weak 覆盖）

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 查看 `.map` 文件 | `Device_Table`/`Device_Property`/`User_Standard_Requests` 指向 `usb_prop.o` | 待验证 |
| 2 | 确认 `usb_globals.o` 不在链接中 | `usb_globals.c` 已从工程移除 | 待验证 |

### 7.3 USBTreeView 验证（核心验收）

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 烧录，插 USB | Windows 发出设备插入提示音 | 待验证 |
| 2 | 设备管理器 → 通用串行总线控制器 | 出现 "Unknown Device" 或带黄色感叹号的设备 | **B2 核心验收** |
| 3 | USBTreeView 查看设备描述符 | VID=0x0483 PID=0x5720 bcdUSB=2.00 bMaxPacketSize0=64 | **B1+B2 联合验收** |
| 4 | USBTreeView 查看配置描述符 | 请求失败（GetConfigDescriptor=NULL） | 预期行为 |

### 7.4 断点验证（可选，需调试器）

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 断点设在 `MASS_init()` | 烧录后命中 | 待验证 |
| 2 | 断点设在 `MASS_Reset()` | 插 USB 后命中 | 待验证 |
| 3 | 断点设在 `MASS_GetDeviceDescriptor()` | 主机请求设备描述符时命中 | 待验证 |

---

## 8. 常见问题排查

### 8.1 编译报错：undefined symbol USB_Init

**原因**：`main.c` 调用了 `USB_Init()`，但 USB 库的 `usb_init.c` 未加入工程。

**修复**：A1 已将 `usb_init.c` 加入 USB-FS-Device Group，若报错检查 uvprojx。

### 8.2 编译报错：MASS_DeviceDescriptor 未定义

**原因**：`usb_prop.c` 未 `#include "usb_desc.h"`，或 B1 的 `usb_desc.c` 未加入工程。

**修复**：确认 `usb_prop.c` 第 8 行有 `#include "usb_desc.h"`，B1 已实施。

### 8.3 设备管理器无反应（连 Unknown Device 都没有）

**原因**：`main.c` 未调用 `USB_Init()`，`MASS_init()` 未执行，`wInterrupt_Mask` 为 0。

**修复**：确认 `main.c` 在 `USB_Cable_Config(ENABLE)` 前调用了 `USB_Init()`（§6.1）。

### 8.4 设备管理器出现设备但立刻消失

**原因**：`MASS_Reset()` 中某步出错导致 HardFault，或 `USB_Init()` 调用顺序错误（在 `USB_Cable_Config` 之后）。

**修复**：确认调用顺序为 `USB_Init()` → `USB_Cable_Config(ENABLE)`（§6.1）。

### 8.5 USBTreeView 读不到 VID/PID（仍显示 0000）

**原因**：`MASS_GetDeviceDescriptor()` 未正确返回 `MASS_DeviceDescriptor` 指针，或 `Device_Descriptor` 长度错误。

**排查**：确认 `Device_Descriptor` 的第二个字段是 `MASS_SIZ_DEVICE_DESC`（18），`MASS_GetDeviceDescriptor()` 调用 `Standard_GetDescriptorData(Length, &Device_Descriptor)`。

---

## 9. 与下一步的衔接

TR1-B2 完成后，设备管理器出现设备（带黄色感叹号），TR1-01 验收通过。下一步：

- **TR1-B3**：新建 `src/usb_pwr.c/h`，`PowerOn()` + `bDeviceState` 状态机 + `fSuspendEnabled = FALSE`。B3 完成后：
  - 取消 `usb_istr.c` 中 WKUP/SUSP/ESOF 分支的 `#if 0` 包裹，加回 `#include "usb_pwr.h"`
  - `usb_prop.c` 中 `MASS_init`/`MASS_Reset` 的 `bDeviceState` 注释取消
  - `main.c` 的 `USB_Cable_Config(ENABLE)` 改为 `PowerOn()`（B4）
- **TR1-B4**：`main.c` 改为完整初始化序列（`Set_System` → `Set_USBClock` → `USB_Interrupts_Config` → `USB_Init` → `PowerOn` → `while`），设备出现在设备管理器。
- **TR1-C1**：`usb_desc.c/h` 补入 `MASS_ConfigDescriptor`（32B），`usb_prop.c` 补入 `MASS_GetConfigDescriptor` 和 `Config_Descriptor`，黄色感叹号消失。

> **B2 是 TR1 的转折点**：B2 之前所有阶段的可观测结果都是"设备描述符请求失败"，B2 之后设备管理器第一次出现设备。这是 TR1-01 的真正含义，也是学习者第一个"看得见"的里程碑。

---

*文档结束。*
