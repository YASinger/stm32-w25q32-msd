# TR1-C3 详细设计：SET_ADDRESS / SET_CONFIGURATION 完整响应

## 修改记录

| 版本 | 日期 | 修改内容 | 修改人 |
|---|---|---|---|
| V1.0 | 2026-07-19 | 初始版本 | Copilot |

---

## 文档信息

| 项目 | 内容 |
|---|---|
| 需求编号 | TR1-C3 |
| 需求描述 | SET_ADDRESS / SET_CONFIGURATION 完整响应（标准请求回调 `Device_Table` 完整实现） |
| 验收标准 | 枚举全程无 STALL，`bDeviceState = CONFIGURED` |
| 所属阶段 | TR1-C — 枚举完整化（横向补全） |
| 前置文档 | `项目框架设计.md`、`TR1框架设计.md` |
| 前置代码 | TR1-C2 字符串描述符已完成，设备身份信息完整 |
| 完成态参照 | `tmp\last_project\src\usb_prop.c` |

---

## 1. 需求分解

TR1-C3 的目标是补全标准请求的用户层回调，让枚举流程的 SET_ADDRESS 和 SET_CONFIGURATION 阶段完整工作。B2 阶段这些回调是空函数（满足链接），C3 填入真实逻辑。

**C3 的核心功能**：
1. `Mass_Storage_SetConfiguration()`：主机 SET_CONFIGURATION 后切换 `bDeviceState` 到 `CONFIGURED`，并清除 EP1/EP2 的 DTOG（Data Toggle）
2. `MASS_Get_Interface_Setting()`：主机 SET_INTERFACE 时校验接口号是否合法

**C3 解决的问题**：B4 的 `while (bDeviceState != CONFIGURED)` 会永远阻塞，因为 `Mass_Storage_SetConfiguration()` 是空函数，`bDeviceState` 不会变为 `CONFIGURED`。C3 实现后，枚举完成时 `bDeviceState = CONFIGURED`，`while` 循环退出。

本任务只修改一个文件：

| 操作 | 文件 | 内容 |
|---|---|---|
| 修改 | `src/usb_prop.c` | `Mass_Storage_SetConfiguration()` 填入真实逻辑 + `MASS_Get_Interface_Setting()` 实现 + `Device_Property.Class_Get_Interface_Setting` 恢复 |

---

## 2. 关键技术决策

### 2.1 `Mass_Storage_SetConfiguration()` 的状态机切换

主机发 SET_CONFIGURATION 后，USB 库的 `usb_core.c` 先处理协议层（设置 `Current_Configuration`），然后调用 `pUser_Standard_Requests->User_SetConfiguration()`，即本函数。

完成态的实现：
```c
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
```

**逻辑**：
- `Current_Configuration != 0`：主机选择了配置 1，设备进入 CONFIGURED 状态，同时清 EP1/EP2 的 DTOG
- `Current_Configuration == 0`：主机取消配置（SET_CONFIGURATION(0)），设备回到 ADDRESSED 状态

### 2.2 `ClearDTOG_TX(ENDP1)` / `ClearDTOG_RX(ENDP2)` 的作用

DTOG（Data Toggle）是 USB 端点的数据切换位，用于保证数据包顺序。每次传输后 DTOG 翻转。SET_CONFIGURATION 时清除 DTOG，让 EP1/EP2 从初始状态开始——这是 USB 规范的要求（配置后端点数据 toggle 复位）。

**TR1 阶段 EP1/EP2 的回调是 `NOP_Process`，不实际传输数据**。但 DTOG 清除仍需执行，为 TR2 的 BOT 传输做准备。

### 2.3 `MASS_Get_Interface_Setting()` 的实现

主机发 SET_INTERFACE 时，USB 库调用 `pProperty->Class_Get_Interface_Setting(Interface, AlternateSetting)` 校验是否支持该接口/备选设置。

完成态的实现：
```c
static RESULT MASS_Get_Interface_Setting(uint8_t Interface, uint8_t AlternateSetting)
{
    if (Interface > 0) return USB_UNSUPPORT;
    return USB_SUCCESS;
}
```

**逻辑**：本项目只有接口 0（`bInterfaceNumber=0`），无备选设置（`bAlternateSetting=0`）。接口号 > 0 返回 UNSUPPORT，否则返回 SUCCESS。

### 2.4 `Device_Property.Class_Get_Interface_Setting` 的恢复

B2 阶段该字段为 0（NULL）。C3 改为 `MASS_Get_Interface_Setting`。若保持 NULL，主机 SET_INTERFACE 时 USB 库调用 NULL 函数指针导致 HardFault——但实际不会发生，因为 USB 库在调用前会检查指针是否为 NULL（`usb_core.c` 的 `Standard_SetInterface()` 中有判断）。

**安全起见仍恢复为真实实现**，与完成态一致。

---

## 3. 涉及组件

```
┌─────────────────────────────────────────────────┐
│  主机 SET_CONFIGURATION 请求                     │
└────────────────────┬────────────────────────────┘
                     │ EP0 控制传输
╔════════════════════╧═════════════════════════════╗
║  device (USB 设备层)                              ║
║  ┌───────────────────────────────────────────┐  ║
║  │ usb_prop.c                                │  ║
║  │  ├─ Mass_Storage_SetConfiguration()       │  ║
║  │  │   ├─ bDeviceState = CONFIGURED         │  ║
║  │  │   ├─ ClearDTOG_TX(ENDP1)               │  ║
║  │  │   └─ ClearDTOG_RX(ENDP2)               │  ║
║  │  ├─ MASS_Get_Interface_Setting()          │  ║
║  │  └─ Device_Property.Class_Get_Interface_Setting│
║  │     从 0 → MASS_Get_Interface_Setting     │  ║
║  └───────────────────────────────────────────┘  ║
╠══════════════════════════════════════════════════╣
║  main.c (B4)                                     ║
║  ┌───────────────────────────────────────────┐  ║
║  │ while (bDeviceState != CONFIGURED)        │  ║
║  │   → C3 后退出 (bDeviceState=CONFIGURED)   │  ║
║  └───────────────────────────────────────────┘  ║
╚══════════════════════════════════════════════════╝
```

---

## 4. 文件清单

### 4.1 修改文件

| 文件 | 修改内容 |
|---|---|
| `src/usb_prop.c` | `Mass_Storage_SetConfiguration()` 填入真实逻辑 + `MASS_Get_Interface_Setting()` 实现 + 前向声明 + `Device_Property.Class_Get_Interface_Setting` 恢复 |

### 4.2 新建文件

无。

### 4.3 工程分组

无变化。

---

## 5. 接口设计

### 5.1 `src/usb_prop.c` 修改

**改动 1**：补 `MASS_Get_Interface_Setting` 前向声明：

```c
/* 在现有前向声明区补入 */
static RESULT MASS_Get_Interface_Setting(uint8_t Interface, uint8_t AlternateSetting);
```

**改动 2**：`Device_Property.Class_Get_Interface_Setting` 从 0 改为 `MASS_Get_Interface_Setting`：

```c
/* 改前 */
0,                            /* Class_Get_Interface_Setting — C3 实现 */
/* 改后 */
MASS_Get_Interface_Setting,  /* Class_Get_Interface_Setting */
```

**改动 3**：补 `MASS_Get_Interface_Setting` 实现（在 `MASS_Status_Out` 后、`MASS_GetDeviceDescriptor` 前）：

```c
static RESULT MASS_Get_Interface_Setting(uint8_t Interface, uint8_t AlternateSetting)
{
    if (Interface > 0) return USB_UNSUPPORT;
    return USB_SUCCESS;
}
```

**改动 4**：`Mass_Storage_SetConfiguration()` 从空函数改为真实实现：

```c
/* 改前 */
static void Mass_Storage_SetConfiguration(void)    { /* C3 实现状态机切换 */ }
/* 改后 */
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
```

---

## 6. 实现要点与风险

### 6.1 C3 的可观测变化

**C2 → C3 的变化**：

| 项 | C2 | C3 | 变化 |
|---|---|---|---|
| Problem Code | 10 | 10（不变） | C4 才解决 |
| `bDeviceState` | ATTACHED（卡在 while 循环） | **CONFIGURED**（while 退出） | ✅ 状态机完整 |
| USBTreeView 表面 | 无变化 | 无变化 | C3 是内部行为 |

**C3 的可观测性**：C3 是内部行为，USBTreeView 表面看不出差异。需要调试器验证：
- `while (bDeviceState != CONFIGURED)` 退出，程序进入 `while(1)`
- `bDeviceState` 值为 5（CONFIGURED）

### 6.2 `ClearDTOG` 的调用安全性

`ClearDTOG_TX(ENDP1)` 和 `ClearDTOG_RX(ENDP2)` 是 USB 库的宏（`usb_regs.h`），操作端点寄存器。EP1/EP2 已在 `MASS_Reset()` 中初始化为 Bulk 类型，调用 `ClearDTOG` 安全。

### 6.3 `Mass_Storage_SetConfiguration` 的调用时机

USB 库在 `Standard_SetConfiguration()`（`usb_core.c`）处理完协议层后调用本函数。此时 `pInformation->Current_Configuration` 已被设置为请求的配置值（1 或 0）。本函数只需根据该值切换状态机。

### 6.4 `bDeviceState` 的 `volatile` 属性

`bDeviceState` 声明为 `__IO uint32_t`（volatile）。`Mass_Storage_SetConfiguration()` 在 USB 中断中赋值 `CONFIGURED`，`main.c` 的 `while` 循环在主线程读取——volatile 确保主线程能看到中断中的赋值，不会用缓存的旧值。

---

## 7. 验证流程

### 7.1 编译验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | Keil Build (F7) | 0 Error(s), 0 Warning(s) | 待验证 |

### 7.2 USBTreeView 验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 烧录，插 USB | 与 C2 表现一致（设备描述符+配置描述符+Summary 字符串） | 待验证 |
| 2 | Problem Code | 仍为 10（C4 才解决） | 预期行为 |

### 7.3 断点验证（可选，需调试器）

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 断点设在 `Mass_Storage_SetConfiguration()` | 插 USB 后命中 | 待验证 |
| 2 | 单步过 `bDeviceState = CONFIGURED` | `bDeviceState` = 5 | 待验证 |
| 3 | 断点设在 `while(1)` | 命中（`while` 循环已退出） | **C3 核心验收** |

---

## 8. 常见问题排查

### 8.1 `while (bDeviceState != CONFIGURED)` 仍不退出

**原因**：`Mass_Storage_SetConfiguration()` 未被调用，或 `Current_Configuration` 为 0。

**排查**：
- 确认 `Device_Property` 的 `User_SetConfiguration` 字段指向 `Mass_Storage_SetConfiguration`（B2 已配置）
- 确认 `Mass_Storage_SetConfiguration()` 已填入真实逻辑（§5.1 改动 4）
- 主机是否发了 SET_CONFIGURATION？USBTreeView 的 `Current Config Value` 应为 0x01

### 8.2 编译报错：identifier "RESULT" is undefined

**原因**：`MASS_Get_Interface_Setting` 的返回类型 `RESULT` 来自 `usb_core.h`，经 `usb_lib.h` 间接包含。

**修复**：确认 `usb_prop.c` 第一行有 `#include "usb_lib.h"`（B2 已有）。

### 8.3 HardFault on SET_INTERFACE

**原因**：`Device_Property.Class_Get_Interface_Setting` 仍为 NULL，但 USB 库调用前未检查。

**修复**：确认该字段已改为 `MASS_Get_Interface_Setting`（§5.1 改动 2）。

---

## 9. 与下一步的衔接

TR1-C3 完成后，标准请求响应完整，`bDeviceState` 能到达 `CONFIGURED`。下一步：

- **TR1-C4**：`usb_prop.c` 补 `MASS_Data_Setup`/`MASS_NoData_Setup`（MSC 类请求 GET_MAX_LUN / Bulk-Only Reset）。Problem Code 10 消失，USBSTOR 驱动完全加载，黄色感叹号消失。
- **TR1-C5**：`USB_Cable_Config(DISABLE/ENABLE)` 软件重连。调用函数后 PC 重新枚举。

> **C3 是状态机的闭环**：B4 的 `while` 循环等 C3 来解除。C3 完成后，程序流程第一次能跑完初始化序列进入主循环——虽然主循环是空的，但这标志着枚举流程在协议层完整走通。

---

*文档结束。*
