# TR1-B3 详细设计：usb_pwr 电源管理与状态机

## 修改记录

| 版本 | 日期 | 修改内容 | 修改人 |
|---|---|---|---|
| V1.0 | 2026-07-18 | 初始版本 | Copilot |

---

## 文档信息

| 项目 | 内容 |
|---|---|
| 需求编号 | TR1-B3 |
| 需求描述 | `usb_pwr.c/h` PowerOn + `bDeviceState` 状态机（UNCONNECTED→ATTACHED→POWERED→DEFAULT） + `fSuspendEnabled=FALSE` |
| 验收标准 | D+ 上拉生效，主机发起 RESET，设备进入 DEFAULT 状态 |
| 所属阶段 | TR1-B — 最小枚举通过（纵向切片） |
| 前置文档 | `项目框架设计.md`、`TR1框架设计.md` |
| 前置代码 | TR1-A 骨架全部完成；TR1-B2 设备属性已接入，PC 可识别设备 |
| 完成态参照 | `tmp\last_project\src\usb_pwr.c`、`tmp\last_project\inc\usb_pwr.h`（TR1 全部完成后的最终形态） |

---

## 1. 需求分解

TR1-B3 是 TR1-B 阶段的第三步，目标是实现 USB 电源管理和设备状态机。B2 让 PC 识别到了设备，但 `main.c` 中用的是 A2 的临时调用 `USB_Cable_Config(ENABLE)` 来使能 D+ 上拉。B3 用正式的 `PowerOn()` 替代这一临时调用，并引入 `bDeviceState` 状态机跟踪设备从 UNCONNECTED 到 CONFIGURED 的完整枚举过程。

**与之前任务的关键区别**：B3 **不需要裁剪**——完成态的 `usb_pwr.c/h` 就是 B3 的目标形态。B3 完成后，之前 A4 和 B2 预留的恢复点全部激活：
- A4 的 `usb_istr.c` 中 WKUP/SUSP/ESOF 三个 `#if 0` 分支取消包裹，加回 `#include "usb_pwr.h"`
- B2 的 `usb_prop.c` 中 `MASS_init`/`MASS_Reset` 的 `bDeviceState` 注释取消

本任务需要新建两个文件，并修改三个现有文件：

| 操作 | 文件 | 内容 |
|---|---|---|
| 新建 | `src/usb_pwr.c` | `bDeviceState`/`fSuspendEnabled` 定义 + `PowerOn`/`PowerOff`/`Suspend`/`Resume` 实现 |
| 新建 | `inc/usb_pwr.h` | `RESUME_STATE`/`DEVICE_STATE` 枚举 + 函数声明 + 全局变量 extern |
| 修改 | `src/usb_istr.c` | 加回 `#include "usb_pwr.h"`，取消 WKUP/SUSP/ESOF 三个 `#if 0` 分支 |
| 修改 | `src/usb_prop.c` | 取消 `MASS_init`/`MASS_Reset` 中 `bDeviceState` 赋值的注释 |
| 修改 | `src/main.c` | `USB_Cable_Config(ENABLE)` 改为 `PowerOn()`（B4 的完整序列前置） |

---

## 2. 关键技术决策

### 2.1 禁用挂起模式（§6.2，TR1 的硬约束）

**核心决策**：`fSuspendEnabled = FALSE`。

**原因**（文档 §6.2 明确记载）：主机在发 RESET 前的总线空闲期会触发 SUSP 中断。若 `fSuspendEnabled` 为 `TRUE`，`Suspend()` 会让 MCU 进入 STOP 模式，但 USBWakeUp 中断已在 A2 的 NVIC 中禁用——MCU 无法唤醒，枚举彻底失败。

**完整链路**（A2/A3/B3 协同）：

| 层 | 组件 | 状态 | 说明 |
|---|---|---|---|
| NVIC | USBWakeUp_IRQn | A2 已禁用 | 硬件层禁止唤醒中断 |
| CNTR 掩码 | `IMR_MSK` 含 `CNTR_SUSPM` | A3 已配置 | SUSP 中断仍被捕获，由软件处理 |
| 软件开关 | `fSuspendEnabled = FALSE` | **B3 本任务** | `USB_Istr()` SUSP 分支判断后走 `Resume(RESUME_LATER)` 而非 `Suspend()` |
| 状态机 | `bDeviceState` 不进 SUSPENDED | B3 本任务 | 枚举流程不被挂起打断 |

**`USB_Istr()` SUSP 分支的行为**（B3 恢复后）：
```c
if (fSuspendEnabled) {
    Suspend();           /* fSuspendEnabled=FALSE, 不走这里 */
} else {
    Resume(RESUME_LATER); /* 走这里, 实际是空操作（ResumeS.eState=RESUME_OFF） */
}
```

`fSuspendEnabled = FALSE` 时，`Resume(RESUME_LATER)` 被调用。看完成态的 `Resume()` 实现：`RESUME_LATER` 分支判断 `remotewakeupon == 0` 后设 `ResumeS.eState = RESUME_OFF`，`RESUME_OFF` 在 switch 的 default 分支不做任何事。所以 SUSP 中断的实际效果是**被安全忽略**——这正是 TR1 想要的行为。

### 2.2 `PowerOn()` 与 `USB_Cable_Config(ENABLE)` 的关系

A2 的 `main.c` 用 `USB_Cable_Config(ENABLE)` 临时使能 D+ 上拉（满足 A2 验收"PA12 已切为 AF_PP"）。B3 的 `PowerOn()` 正式接管这一职责，并做了更多事：

| 步骤 | 操作 | 说明 |
|---|---|---|
| [1] | `USB_Cable_Config(ENABLE)` | D+ 上拉使能（与 A2 相同） |
| [2] | `SetCNTR(CNTR_FRES)` | 强制复位 USB 外设，清除 `USB_SIL_Init` 残留状态 |
| [3] | `SetCNTR(0)` | 清除强制复位 |
| [4] | `SetISTR(0)` + `SetCNTR(IMR_MSK)` | 清中断标志 + 使能 USB 中断 |
| [5] | `bDeviceState = ATTACHED` | 状态机进入已连接 |

**关键差异**：`USB_Cable_Config(ENABLE)` 只做了步骤 [1]，而 `PowerOn()` 还做了 [2]~[5]——特别是步骤 [4] 的 `SetCNTR(IMR_MSK)`，它使能了 USB 中断。B2 阶段 `MASS_init()` 的 `USB_SIL_Init()` 也设置了 `wInterrupt_Mask = IMR_MSK`，但 `PowerOn()` 的 `SetCNTR(IMR_MSK)` 是**重复确认**，确保 CNTR 寄存器真正写入。

### 2.3 `bDeviceState` 状态机

`bDeviceState` 跟踪 USB 设备的完整状态（文档 §3.2 B3 行）：

```
UNCONNECTED → ATTACHED → POWERED → DEFAULT → ADDRESSED → CONFIGURED
```

| 状态 | 触发时机 | 设置者 |
|---|---|---|
| UNCONNECTED | 上电初始 / PowerOff | `usb_pwr.c` 定义初始值 / `PowerOff()` |
| ATTACHED | D+ 上拉生效 | `PowerOn()` |
| POWERED | 主机发 RESET | `usb_prop.c` `MASS_Reset()`（B2 预留，B3 激活） |
| DEFAULT | RESET 完成，等待 SET_ADDRESS | USB 库内部 |
| ADDRESSED | SET_ADDRESS 完成 | USB 库内部 |
| CONFIGURED | SET_CONFIGURATION 完成 | `usb_prop.c` `Mass_Storage_SetConfiguration()`（C3） |

B3 阶段只涉及前三个状态（UNCONNECTED/ATTACHED/POWERED），DEFAULT/ADDRESSED/CONFIGURED 由 USB 库和 C3 处理。

### 2.4 `Suspend()` 的实现保留但不调用

完成态的 `Suspend()` 包含 `PWR_EnterSTOPMode()`。虽然 `fSuspendEnabled = FALSE` 意味着 `Suspend()` 永远不会被 `USB_Istr()` 调用，但代码保留——这是 TR2 可能重新启用挂起模式的预留。

**注意**：`Suspend()` 调用了 `PWR_EnterSTOPMode()`，这需要 `stm32f10x_pwr.h`（StdPeriph 库）——A1 已全量集成，无额外依赖。

---

## 3. 涉及组件

```
┌─────────────────────────────────────────────────┐
│  main.c (B4 将串联完整流程)                      │
│  B3: Set_System → Set_USBClock →                 │
│      USB_Interrupts_Config → USB_Init → PowerOn  │
└────────────────────┬────────────────────────────┘
                     │
╔════════════════════╧═════════════════════════════╗
║  power (USB 设备层)                               ║
║  ┌───────────────────────────────────────────┐  ║
║  │ usb_pwr.c / usb_pwr.h                     │  ║
║  │  ├─ bDeviceState          设备状态机      │  ║
║  │  ├─ fSuspendEnabled=FALSE  禁用挂起       │  ║
║  │  ├─ PowerOn()             上电+D+上拉     │  ║
║  │  ├─ PowerOff()            断电            │  ║
║  │  ├─ Suspend()             挂起(保留不用)  │  ║
║  │  └─ Resume()              唤醒(安全忽略)  │  ║
║  └───────────────────────────────────────────┘  ║
╠══════════════════════════════════════════════════╣
║  被引用 (B3 激活的恢复点)                        ║
║  ┌──────────────────────────────────────────┐   ║
║  │ usb_istr.c (A4)                          │   ║
║  │  WKUP/SUSP/ESOF 分支 ← Resume/Suspend    │   ║
║  └──────────────────────────────────────────┘   ║
║  ┌──────────────────────────────────────────┐   ║
║  │ usb_prop.c (B2)                          │   ║
║  │  MASS_init/MASS_Reset ← bDeviceState     │   ║
║  └──────────────────────────────────────────┘   ║
╚══════════════════════════════════════════════════╝
```

---

## 4. 文件清单

### 4.1 新建文件

| 文件 | 说明 |
|---|---|
| `src/usb_pwr.c` | `bDeviceState`/`fSuspendEnabled` 定义 + 4 个函数实现 |
| `inc/usb_pwr.h` | 枚举类型 + 函数声明 + 全局变量 extern |

### 4.2 修改文件

| 文件 | 修改内容 |
|---|---|
| `src/usb_istr.c` | 加回 `#include "usb_pwr.h"`，取消 WKUP/SUSP/ESOF 三个 `#if 0` 分支 |
| `src/usb_prop.c` | 取消 `MASS_init`/`MASS_Reset` 中 `bDeviceState` 赋值的注释，加回 `#include "usb_pwr.h"` |
| `src/main.c` | `USB_Cable_Config(ENABLE)` 改为 `PowerOn()` |
| `project.uvprojx` | src Group 加入 `usb_pwr.c` 和 `usb_pwr.h` |

---

## 5. 接口设计

### 5.1 `inc/usb_pwr.h`

```c
#ifndef __USB_PWR_H
#define __USB_PWR_H

#include "stm32f10x.h"
#include "usb_type.h"

typedef enum _RESUME_STATE {
    RESUME_OFF = -1,
    RESUME_ESOF = 0,
    RESUME_EXTERNAL,
    RESUME_INTERNAL,
    RESUME_LATER,
    RESUME_WAIT,
    RESUME_ON
} RESUME_STATE;

typedef enum _DEVICE_STATE {
    UNCONNECTED = 0,
    ATTACHED    = 1,
    POWERED     = 2,
    SUSPENDED   = 3,
    ADDRESSED   = 4,
    CONFIGURED  = 5
} DEVICE_STATE;

extern __IO uint32_t bDeviceState;
extern __IO bool     fSuspendEnabled;

void PowerOn(void);
void PowerOff(void);
void Suspend(void);
void Resume(RESUME_STATE eResumeSetVal);

#endif /* __USB_PWR_H */
```

**说明**：与完成态完全一致。`RESUME_OFF = -1` 是完成态特有的设计（参考例程从 0 开始），`Resume()` 的 switch 中 default 分支处理 `RESUME_OFF` 为空操作。

### 5.2 `src/usb_pwr.c`

与完成态完全一致，直接复制 `tmp\last_project\src\usb_pwr.c` 的内容（§1 已展示）。不需要任何裁剪。

### 5.3 `src/usb_istr.c` 修改

**改动 1**：加回 `#include "usb_pwr.h"`：
```c
#include "usb_lib.h"
#include "usb_pwr.h"   /* B3 加回 */
```

**改动 2**：取消三个 `#if 0` 分支的包裹。删除 `#if 0` 和对应的 `#endif` 两行，恢复完成态的 8 分支完整结构。

### 5.4 `src/usb_prop.c` 修改

**改动 1**：加回 `#include "usb_pwr.h"`：
```c
#include "usb_lib.h"
#include "usb_desc.h"
#include "usb_pwr.h"   /* B3 加回 */
```

**改动 2**：取消 `MASS_init()` 中的注释：
```c
/* 改前 */
/* bDeviceState = UNCONNECTED; */  /* B3 实现 (usb_pwr.h) */
/* 改后 */
bDeviceState = UNCONNECTED;
```

**改动 3**：取消 `MASS_Reset()` 末尾的注释：
```c
/* 改前 */
/* bDeviceState = ATTACHED; */  /* B3 实现 (usb_pwr.h) */
/* 改后 */
bDeviceState = ATTACHED;
```

### 5.5 `src/main.c` 修改

```c
/* 改前 */
USB_Cable_Config(ENABLE);    /* PA12 切为 AF_PP, D+ 上拉生效 */

/* 改后 */
PowerOn();                   /* D+ 上拉使能 + USB 外设复位 + 中断使能 */
```

同时加回 `#include "usb_pwr.h"`：
```c
#include "stm32f10x.h"
#include "hw_config.h"
#include "usb_lib.h"
#include "usb_pwr.h"   /* B3 加回 */
```

---

## 6. 实现要点与风险

### 6.1 `PowerOn()` 与 `USB_SIL_Init()` 的重复设置

**现象**：`MASS_init()`（B2）调用 `USB_SIL_Init()` 设置 `wInterrupt_Mask = IMR_MSK` 并写 CNTR。`PowerOn()`（B3）又做了一遍 `SetCNTR(IMR_MSK)`。

**分析**：这不是冗余，是**防御性确认**。`USB_SIL_Init()` 在 `USB_Init()` 中被调用（B2 的 `main.c`），而 `PowerOn()` 在 `main.c` 的 `USB_Init()` 之后调用。两次设置确保 CNTR 寄存器最终值正确——即使 `USB_SIL_Init()` 和 `PowerOn()` 之间有其他代码意外修改了 CNTR。

### 6.2 B3 的可观测性变化

**B2 → B3 的可观测差异**：

| 项 | B2 | B3 | 变化 |
|---|---|---|---|
| `main.c` 末尾 | `USB_Cable_Config(ENABLE)` | `PowerOn()` | 内部行为变化 |
| `usb_istr.c` 分支数 | 5 个（CTR/RESET/DOVR/ERR/SOF） | 8 个（+WKUP/SUSP/ESOF） | SUSP 中断现在被处理（安全忽略） |
| `bDeviceState` | 未定义 | 已定义并跟踪状态 | 状态机激活 |
| USBTreeView 表现 | Problem Code 43 | **可能改善** | `PowerOn()` 的 CNTR 配置可能改善枚举稳定性 |

**关键预期**：B3 的 `PowerOn()` 做了 `SetCNTR(CNTR_FRES)` → `SetCNTR(0)` → `SetCNTR(IMR_MSK)` 的完整 CNTR 配置序列，这比 B2 的 `USB_SIL_Init()` 单独设置更完整。**可能**改善 B2 的 Problem Code 43（枚举失败）——因为 B2 的 CNTR 配置可能不完整。

但不能保证 Problem Code 43 消失——`GetConfigDescriptor` 仍为 NULL（C1 才补），主机 GET_DESCRIPTOR(Config) 仍会失败。B3 的预期是**枚举过程更稳定**，而不是枚举完全成功。

### 6.3 `Suspend()` 的 `PWR_EnterSTOPMode` 依赖

`Suspend()` 调用 `PWR_EnterSTOPMode()`（StdPeriph 库的 `stm32f10x_pwr.c`）。虽然 `fSuspendEnabled = FALSE` 意味着 `Suspend()` 不会被调用，但链接器仍需要 `PWR_EnterSTOPMode` 符号。A1 已全量集成 StdPeriph 库，无额外依赖。

---

## 7. 验证流程

### 7.1 编译验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | Keil Build (F7) | 0 Error(s), 0 Warning(s) | 待验证 |

### 7.2 USBTreeView 验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 烧录，插 USB | 设备管理器仍出现设备 | 待验证 |
| 2 | USBTreeView 查看 | 与 B2 表现对比，Problem Code 可能改善 | 待验证 |
| 3 | 观察枚举稳定性 | 多次拔插，枚举结果一致 | 待验证 |

### 7.3 断点验证（可选，需调试器）

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 断点设在 `PowerOn()` | 烧录后命中 | 待验证 |
| 2 | 查看 `bDeviceState` 值 | `PowerOn()` 后 = ATTACHED (1) | 待验证 |
| 3 | 断点设在 `USB_Istr()` SUSP 分支 | 主机空闲期命中，走 `Resume(RESUME_LATER)` 而非 `Suspend()` | 待验证 |

---

## 8. 常见问题排查

### 8.1 编译报错：cannot open source input file "usb_pwr.h"

**原因**：`usb_pwr.h` 未创建，或 uvprojx 的 src Group 未加入。

**修复**：确认 `inc/usb_pwr.h` 存在，uvprojx 已加入（§4.1）。

### 8.2 编译报错：identifier "bool" is undefined

**原因**：`usb_pwr.h` 中 `extern __IO bool fSuspendEnabled;` 的 `bool` 类型来自 `usb_type.h`，但 `usb_pwr.h` 的 include 顺序导致 `usb_type.h` 未先被包含。

**修复**：`usb_pwr.h` 已 `#include "usb_type.h"`（§5.1），确认该行存在。

### 8.3 链接报错：Undefined symbol PWR_EnterSTOPMode

**原因**：`Suspend()` 调用了 `PWR_EnterSTOPMode()`，但 `stm32f10x_pwr.c` 未加入工程。

**修复**：A1 已全量集成 StdPeriph 库，若报错检查 uvprojx 的 StdPeriph Group 是否包含 `stm32f10x_pwr.c`。

### 8.4 枚举比 B2 更不稳定

**原因**：`main.c` 中 `PowerOn()` 的调用时机可能太晚（在 `USB_Init()` 之后），或 `PowerOn()` 的 CNTR 配置与 `USB_SIL_Init()` 冲突。

**排查**：确认 `main.c` 的调用顺序为 `USB_Init()` → `PowerOn()`（§5.5）。`USB_Init()` 先初始化设备表，`PowerOn()` 再配置 CNTR。

---

## 9. 与下一步的衔接

TR1-B3 完成后，电源管理和状态机就位。下一步：

- **TR1-B4**：`main.c` 改为完整初始化序列（`Set_System` → `Set_USBClock` → `USB_Interrupts_Config` → `USB_Init` → `PowerOn` → `while(bDeviceState != CONFIGURED)` → `while(1)`）。B4 的 `while(bDeviceState != CONFIGURED)` 等待枚举完成，这是 B3 状态机的实际应用。
- **TR1-C1**：`usb_desc.c/h` 补入 `MASS_ConfigDescriptor`（32B），`usb_prop.c` 补入 `MASS_GetConfigDescriptor` 和 `Config_Descriptor`，黄色感叹号消失。

> **B3 是 TR1-B 阶段的收尾**：B1 提供数据，B2 接入机制，B3 完善电源管理。B4 只是把 `main.c` 串成完整流程，没有新组件。B3+B4 合起来才是"最小枚举通过"的完整含义。

---

*文档结束。*
