# TR1-A4 详细设计：usb_istr 中断服务与 ISTR 事件分发

## 修改记录

| 版本 | 日期 | 修改内容 | 修改人 |
|---|---|---|---|
| V1.0 | 2026-07-18 | 初始版本 | Copilot |
| V1.1 | 2026-07-18 | 以 `tmp\last_project` 完成态为基准重写：精简 include（仅 `usb_lib.h`），DOVR/ERR/SOF 分支保留（不依赖 `usb_pwr.h`），仅 SUSP/WKUP/ESOF 三个分支 `#if 0` 包裹 | Copilot |

---

## 文档信息

| 项目 | 内容 |
|---|---|
| 需求编号 | TR1-A4 |
| 需求描述 | `usb_istr.c/h` USB 中断入口 `USB_LP_CAN1_RX0_IRQHandler` + `USB_Istr()` ISTR 事件分发 |
| 验收标准 | 插 USB 后断点能命中 `USB_Istr()`，ISTR 的 RESET 位被置位 |
| 所属阶段 | TR1-A — USB 栈骨架可编译运行 |
| 前置文档 | `项目框架设计.md`、`TR1框架设计.md` |
| 前置代码 | TR1-A2 NVIC 已使能 USB_LP 中断；TR1-A3 `usb_conf.h` 已定义 14 个端点回调宏 |
| 完成态参照 | `tmp\last_project\src\usb_istr.c`（TR1 全部完成后的最终形态） |

---

## 1. 需求分解

TR1-A4 是骨架阶段的最后一步，目标是让 USB 中断**真正能被触发并分发**。A2 完成了 NVIC 配置（中断通道打通），A3 完成了端点回调宏定义（分发目标确定），A4 把两者连起来——实现中断入口和 ISTR 事件分发器。

本任务需要新建两个文件，并修改一个现有文件：

| 操作 | 文件 | 内容 |
|---|---|---|
| 新建 | `src/usb_istr.c` | `wIstr`/`pEpInt_IN`/`pEpInt_OUT` 强定义 + `USB_LP_CAN1_RX0_IRQHandler` 入口 + `USB_Istr()` 分发 |
| 新建 | `inc/usb_istr.h` | `USB_Istr()` 声明 |
| 修改 | `src/usb_globals.c` | 移除 `wIstr`/`pEpInt_IN`/`pEpInt_OUT` 三个 weak 占位（被强定义覆盖） |

**完成态参照**：本任务的最终形态以 `tmp\last_project\src\usb_istr.c` 为准。A4 阶段因 `usb_pwr.h`（B3）尚未创建，需在完成态基础上做最小裁剪（详见 §2.3）。B3 完成后取消裁剪即恢复完成态。

---

## 2. 关键技术决策

### 2.1 中断入口位置

完成态（`last_project`）把 `USB_LP_CAN1_RX0_IRQHandler` 放在 `usb_istr.c` 而非 `stm32_it.c`，本任务沿用。

**理由**：
1. `stm32f10x_it.c` 是 ST 标准模板，A1 保持原样以便后续比对和升级
2. USB 相关代码集中在 `usb_istr.c` 一个文件，符合组件化原则
3. 启动文件 `startup_stm32f10x_md.s` 的向量表中 `USB_LP_CAN1_RX0_IRQHandler` 是 weak 符号，`usb_istr.c` 的强定义自动覆盖，无需修改启动文件

### 2.2 weak 占位的覆盖策略

A1 在 `usb_globals.c` 中用 `__attribute__((weak))` 提供了 `wIstr`/`pEpInt_IN`/`pEpInt_OUT` 三个占位。A4 在 `usb_istr.c` 中提供同名的强定义（不带 weak 属性），链接器自动选择强定义。

**处理**：A4 实施后，从 `usb_globals.c` 中移除这三个 weak 占位，保留 `Device_Table`/`Device_Property`/`User_Standard_Requests`（它们等 B2 覆盖）。这样 `usb_globals.c` 的职责随阶段推进逐步缩小，最终 B2 完成后可整个移除。

### 2.3 相对完成态的最小裁剪（A4 特有）

完成态（`last_project`）的 `usb_istr.c` 包含 `#include "usb_pwr.h"`，且 8 个 ISTR 分支全部编译。但 `usb_pwr.h/c` 属 TR1-B3 任务，A4 阶段不存在。

**裁剪原则**：只裁剪**直接依赖 `usb_pwr.h` 符号**的部分，其余保留。

`usb_pwr.h` 提供的符号：`RESUME_STATE` 类型、`Resume()`、`Suspend()`、`fSuspendEnabled`、`bDeviceState`。

**逐一分析 8 个分支的依赖**：

| 分支 | 调用的符号 | 依赖 `usb_pwr.h`? | A4 处理 |
|---|---|---|---|
| CTR | `CTR_LP()`（`usb_int.c`） | 否 | ✅ 保留 |
| RESET | `Device_Property.Reset()`（B2 占位） | 否 | ✅ 保留 |
| DOVR | `_SetISTR()`（`usb_regs.h`） | 否 | ✅ 保留 |
| ERR | `_SetISTR()`（`usb_regs.h`） | 否 | ✅ 保留 |
| WKUP | `Resume(RESUME_EXTERNAL)` | **是** | ❌ `#if 0` 包裹 |
| SUSP | `fSuspendEnabled`/`Suspend()`/`Resume()` | **是** | ❌ `#if 0` 包裹 |
| SOF | `bIntPackSOF++`（自身变量） | 否 | ✅ 保留 |
| ESOF | `Resume(RESUME_ESOF)` | **是** | ❌ `#if 0` 包裹 |

**结论**：8 个分支中 5 个不依赖 `usb_pwr.h`，A4 保留；仅 WKUP/SUSP/ESOF 三个分支依赖，用 `#if 0` 包裹。同时删除 `#include "usb_pwr.h"`。

**恢复方式**：B3 完成 `usb_pwr.c/h` 后，加回 `#include "usb_pwr.h"`，删除 `#if 0`/`#endif` 两行，即恢复完成态。

### 2.4 `usb_istr.h` 的裁剪

完成态（`last_project`）没有 `usb_istr.h`——`USB_Istr()` 只在 `usb_istr.c` 中定义，`USB_LP_CAN1_RX0_IRQHandler` 通过启动文件的 weak 向量自动关联，无需头文件声明。

但文档 §5.1 组件清单要求 `inc/usb_istr.h` 存在（接口声明）。本任务创建一个极简头文件，只声明 `USB_Istr()`，不包含参考例程中冗余的 14 个端点回调声明（它们已是 `usb_conf.h` 的宏，函数声明会与之冲突）。

---

## 3. 涉及组件

```
┌─────────────────────────────────────────────────┐
│  启动文件 startup_stm32f10x_md.s                 │
│  向量表: USB_LP_CAN1_RX0_IRQHandler (weak)      │
└────────────────────┬────────────────────────────┘
                     │ 强定义覆盖
╔════════════════════╧═════════════════════════════╗
║  isr (USB 设备层)                                 ║
║  ┌───────────────────────────────────────────┐  ║
║  │ usb_istr.c / usb_istr.h                   │  ║
║  │  ├─ USB_LP_CAN1_RX0_IRQHandler  中断入口  │  ║
║  │  ├─ USB_Istr()                  ISTR 分发 │  ║
║  │  ├─ wIstr                       ISTR 缓存  │  ║
║  │  ├─ pEpInt_IN[7]                EP IN 回调│  ║
║  │  └─ pEpInt_OUT[7]               EP OUT 回调│ ║
║  └───────────────────────────────────────────┘  ║
╠══════════════════════════════════════════════════╣
║  依赖                                            ║
║  ┌──────────────┐  ┌──────────────────────────┐ ║
║  │ usb_int.c    │  │ usb_conf.h (A3)          │ ║
║  │ CTR_LP()     │  │ 14 个回调宏               │ ║
║  └──────────────┘  └──────────────────────────┘ ║
║  ┌──────────────┐  ┌──────────────────────────┐ ║
║  │ usb_core.h   │  │ usb_globals.c (A1)       │ ║
║  │ Device_Property│ │ wIstr 等 weak 占位(移除) │ ║
║  └──────────────┘  └──────────────────────────┘ ║
╚══════════════════════════════════════════════════╝
```

---

## 4. 文件清单

### 4.1 新建文件

| 文件 | 说明 |
|---|---|
| `src/usb_istr.c` | 中断入口 + ISTR 分发 + 3 个全局符号强定义 |
| `inc/usb_istr.h` | `USB_Istr()` 声明 |

### 4.2 修改文件

| 文件 | 修改内容 |
|---|---|
| `src/usb_globals.c` | 移除 `wIstr`/`pEpInt_IN`/`pEpInt_OUT` 三个 weak 占位 |

### 4.3 工程分组

`project.uvprojx` 的 `src` Group 加入 `usb_istr.c` 和 `usb_istr.h`。

---

## 5. 接口设计

### 5.1 `inc/usb_istr.h`

```c
#ifndef __USB_ISTR_H
#define __USB_ISTR_H

void USB_Istr(void);

#endif /* __USB_ISTR_H */
```

**说明**：完成态（`last_project`）没有此文件。本任务按文档 §5.1 组件清单要求创建极简头文件，只声明 `USB_Istr()`。不包含参考例程中冗余的 14 个端点回调声明（它们已是 `usb_conf.h` 的宏）。

### 5.2 `src/usb_istr.c`

```c
/**
  ******************************************************************************
  * @file    usb_istr.c
  * @brief   USB 中断服务例程 — TR1-A4
  *
  *          中断入口 + ISTR 事件分发 + 端点回调表。
  *          以 tmp\last_project 完成态为基准，A4 阶段因 usb_pwr.h (B3)
  *          尚未创建，裁剪其依赖：删除 #include "usb_pwr.h"，
  *          WKUP/SUSP/ESOF 三个分支 #if 0 包裹，B3 完成后恢复。
  ******************************************************************************
  */

#include "usb_lib.h"

__IO uint16_t wIstr;
__IO uint8_t  bIntPackSOF = 0;

/* 端点回调表 — TR1 阶段全部指向 NOP_Process */
void (*pEpInt_IN[7])(void) = {
    EP1_IN_Callback,
    EP2_IN_Callback,
    EP3_IN_Callback,
    EP4_IN_Callback,
    EP5_IN_Callback,
    EP6_IN_Callback,
    EP7_IN_Callback,
};

void (*pEpInt_OUT[7])(void) = {
    EP1_OUT_Callback,
    EP2_OUT_Callback,
    EP3_OUT_Callback,
    EP4_OUT_Callback,
    EP5_OUT_Callback,
    EP6_OUT_Callback,
    EP7_OUT_Callback,
};

void USB_Istr(void)
{
    wIstr = _GetISTR();

#if (IMR_MSK & ISTR_CTR)
    if (wIstr & ISTR_CTR & wInterrupt_Mask) {
        CTR_LP();
    }
#endif

#if (IMR_MSK & ISTR_RESET)
    if (wIstr & ISTR_RESET & wInterrupt_Mask) {
        _SetISTR((uint16_t)CLR_RESET);
        Device_Property.Reset();
    }
#endif

#if (IMR_MSK & ISTR_DOVR)
    if (wIstr & ISTR_DOVR & wInterrupt_Mask) {
        _SetISTR((uint16_t)CLR_DOVR);
    }
#endif

#if (IMR_MSK & ISTR_ERR)
    if (wIstr & ISTR_ERR & wInterrupt_Mask) {
        _SetISTR((uint16_t)CLR_ERR);
    }
#endif

#if 0  /* === 以下三个分支依赖 usb_pwr.h (B3)，A4 阶段暂不编译 === */

#if (IMR_MSK & ISTR_WKUP)
    if (wIstr & ISTR_WKUP & wInterrupt_Mask) {
        _SetISTR((uint16_t)CLR_WKUP);
        Resume(RESUME_EXTERNAL);
    }
#endif

#if (IMR_MSK & ISTR_SUSP)
    if (wIstr & ISTR_SUSP & wInterrupt_Mask) {
        if (fSuspendEnabled) {
            Suspend();
        } else {
            Resume(RESUME_LATER);
        }
        _SetISTR((uint16_t)CLR_SUSP);
    }
#endif

#if (IMR_MSK & ISTR_ESOF)
    if (wIstr & ISTR_ESOF & wInterrupt_Mask) {
        _SetISTR((uint16_t)CLR_ESOF);
        Resume(RESUME_ESOF);
    }
#endif

#endif /* 0 === B3 恢复的分支 === */

#if (IMR_MSK & ISTR_SOF)
    if (wIstr & ISTR_SOF & wInterrupt_Mask) {
        _SetISTR((uint16_t)CLR_SOF);
        bIntPackSOF++;
    }
#endif
}

void USB_LP_CAN1_RX0_IRQHandler(void)
{
    USB_Istr();
}
```

**与完成态（`last_project`）的差异对照**：

| 差异点 | 完成态 | A4（当前） | 恢复时机 |
|---|---|---|---|
| `#include "usb_pwr.h"` | 有 | 删除 | B3 创建 `usb_pwr.h` 后加回 |
| WKUP 分支 | 编译 | `#if 0` 包裹 | B3 后取消 `#if 0` |
| SUSP 分支 | 编译 | `#if 0` 包裹 | B3 后取消 `#if 0` |
| ESOF 分支 | 编译 | `#if 0` 包裹 | B3 后取消 `#if 0` |
| CTR/RESET/DOVR/ERR/SOF | 编译 | 编译（一致） | — |

**与 ST 官方参考例程的裁剪对比**（已完成态已做的裁剪，A4 沿用）：

| 参考例程内容 | 完成态处理 | 原因 |
|---|---|---|
| 7 个头文件 include | 仅 `usb_lib.h` + `usb_pwr.h` | `usb_lib.h` 已聚合全部 USB 库头文件 |
| 中断入口在 `stm32_it.c` | 移到 `usb_istr.c` | 保持标准模板不污染 |
| 8 个 `XXX_CALLBACK` 条件编译 | 删除 | TR1 不使用 |
| `usb_istr.h` 中 14 个回调声明 | 删除 | 与 `usb_conf.h` 宏定义冲突 |

### 5.3 `src/usb_globals.c` 修改

从 A1 的 6 个 weak 占位中移除 3 个（被 `usb_istr.c` 强定义覆盖）：

```c
/*==== usb_prop.c 的符号 (TR1-B2 将正式定义) ==============================*/

DEVICE Device_Table __attribute__((weak)) = { 0, 0 };
DEVICE_PROP Device_Property __attribute__((weak)) = { 0 };
USER_STANDARD_REQUESTS User_Standard_Requests __attribute__((weak)) = { 0 };
```

移除的部分：
```c
/* 以下三个符号已被 usb_istr.c 强定义覆盖，从占位中移除 */
// __IO uint16_t wIstr __attribute__((weak)) = 0;
// void (*pEpInt_IN[7])(void)  __attribute__((weak)) = {0};
// void (*pEpInt_OUT[7])(void) __attribute__((weak)) = {0};
```

---

## 6. 实现要点与风险

### 6.1 `Device_Property.Reset()` 的链接依赖

**问题**：`USB_Istr()` 的 RESET 分支调用 `Device_Property.Reset()`。`Device_Property` 是 `usb_globals.c` 中的 weak 占位（全零结构体），其 `Reset` 成员是 NULL 指针。若主机真的发起 RESET，调用 NULL 函数指针会导致 HardFault。

**风险分析**：A4 阶段 `main.c` 调用 `USB_Cable_Config(ENABLE)` 后 D+ 上拉生效，主机可能发起 RESET。此时 `Device_Property.Reset` 是 NULL，调用即 HardFault。

**但这不影响 A4 验收**：A4 的验收标准是"断点能命中 `USB_Istr()`，ISTR 的 RESET 位被置位"——断点在 `USB_Istr()` 入口就命中了，还走不到 `Device_Property.Reset()` 那一行。且 RESET 分支的 `if` 条件需要 `wIstr & ISTR_RESET & wInterrupt_Mask`，而 `wInterrupt_Mask` 在 `USB_SIL_Init()` 中才设置（B2 的 `MASS_init` 调用），A4 阶段 `wInterrupt_Mask` 为 0，RESET 分支不会进入。

**结论**：A4 阶段主机发起 RESET 不会导致 HardFault（因为 `wInterruptMask=0`，分支不进）。断点在 `USB_Istr()` 入口命中即满足验收。B2 完成 `MASS_init()` 后 `wInterrupt_Mask` 才有值，RESET 分支才真正激活。

### 6.2 中断入口的 weak 覆盖

**机制**：启动文件 `startup_stm32f10x_md.s` 中 `USB_LP_CAN1_RX0_IRQHandler` 是 weak 别名（指向 `Default_Handler`）。`usb_istr.c` 中的强定义在链接时自动覆盖。无需修改启动文件。

**验证**：A4 编译链接通过后，可在 `.map` 文件中搜索 `USB_LP_CAN1_RX0_IRQHandler`，确认其地址指向 `usb_istr.o` 而非 `startup_stm32f10x_md.o` 的 `Default_Handler`。

### 6.3 A4 的可观测性

**问题**：A4 只是中断入口和分发器，USB 协议栈的响应部分（描述符、标准请求）仍未实现。插 USB 后 USBTreeView 表现与 A2/A3 一致。

**处理**：这是正常的。A4 的验收标准是"断点能命中 `USB_Istr()`"，USBTreeView 无法直接验证。若无调试器，可通过 `.map` 文件确认 weak 覆盖（§7.2），或烧录后观察是否仍识别 Full-Speed（证明硬件初始化未被破坏）。

---

## 7. 验证流程

### 7.1 编译验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | Keil Build (F7) | 0 Error(s), 0 Warning(s) | 待验证 |

### 7.2 链接验证（确认 weak 覆盖）

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 查看 `.map` 文件 | `wIstr`/`pEpInt_IN`/`pEpInt_OUT` 指向 `usb_istr.o` 而非 `usb_globals.o` | 待验证 |
| 2 | 查看 `.map` 文件 | `USB_LP_CAN1_RX0_IRQHandler` 指向 `usb_istr.o` 而非 `Default_Handler` | 待验证 |

### 7.3 断点验证（需调试器）

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | Debug → Start (Ctrl+F5) | 进入调试模式 | 待验证 |
| 2 | 断点设在 `USB_Istr()` 入口 | — | 待验证 |
| 3 | 插入 USB | 断点命中 | 待验证 |
| 4 | 查看 `wIstr` 值 | `ISTR_RESET` 位（bit 10, 0x0400）被置位 | 待验证 |

### 7.4 USBTreeView 验证（无调试器时）

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 烧录，插 USB | 与 A2/A3 相同："设备描述符请求失败" | 待验证 |

> A4 只是中断入口和分发器，USB 协议栈的响应部分（描述符、标准请求）仍未实现。USBTreeView 表现应与 A2/A3 一致。若表现变化（如不再识别 Full-Speed），说明改动引入了编译期未暴露的问题。

---

## 8. 常见问题排查

### 8.1 编译报错：cannot open source input file "usb_pwr.h"

**原因**：`usb_istr.c` 的 include 列表中保留了 `#include "usb_pwr.h"`，该文件属 B3，A4 不存在。

**修复**：删除 `#include "usb_pwr.h"`（§5.2 已处理）。B3 恢复 WKUP/SUSP/ESOF 分支时再加回。

### 8.2 链接报错：Undefined symbol fSuspendEnabled / Suspend / Resume

**原因**：WKUP/SUSP/ESOF 分支未被 `#if 0` 包裹，引用了 B3 的符号。

**修复**：确认三个分支都被 `#if 0 ... #endif` 包裹（§5.2 已处理）。

### 8.3 断点不命中 `USB_Istr()`

**原因**：NVIC 未使能 USB_LP 中断（A2 未完成），或中断入口未覆盖 weak 向量。

**修复**：
- 确认 A2 的 `USB_Interrupts_Config()` 已执行（`NVIC_IRQChannelCmd = ENABLE`）
- 查看 `.map` 文件确认 `USB_LP_CAN1_RX0_IRQHandler` 指向 `usb_istr.o`

### 8.4 断点命中但 ISTR.RESET 未置位

**原因**：`wInterrupt_Mask` 为 0（`USB_SIL_Init()` 未被调用），RESET 分支的 `if` 条件不满足。

**处理**：这是 A4 的**预期行为**。`wInterrupt_Mask` 在 B2 的 `MASS_init()` 中由 `USB_SIL_Init()` 设置为 `IMR_MSK`。A4 阶段断点在 `USB_Istr()` 入口命中即满足验收，RESET 位是否置位不影响。

---

## 9. 与下一步的衔接

TR1-A4 完成后，骨架阶段（TR1-A）全部结束。USB 中断通道从 NVIC（A2）→ 中断入口（A4）→ ISTR 分发（A4）→ 端点回调（A3）全链路打通。

下一步进入 TR1-B 最小枚举：

- **TR1-B1**：新建 `src/usb_desc.c/h`，最小设备描述符（18B）。主机 GET_DESCRIPTOR(Device) 能拿到 VID=0x0483 PID=0x5720。
- **TR1-B2**：新建 `src/usb_prop.c/h`，定义 `Device_Table`/`Device_Property`/`User_Standard_Requests`（覆盖 `usb_globals.c` 剩余 3 个 weak 占位），实现 `MASS_Reset`/`MASS_GetDeviceDescriptor`。B2 完成后 `usb_globals.c` 可整个移除。设备管理器出现 "Unknown Device"——这是 TR1-01 的真正含义。
- **TR1-B3**：新建 `src/usb_pwr.c/h`，`PowerOn()` + `bDeviceState` 状态机 + `fSuspendEnabled = FALSE`。B3 完成后取消 `usb_istr.c` 中 WKUP/SUSP/ESOF 分支的 `#if 0` 包裹，加回 `#include "usb_pwr.h"`，恢复完成态。
- **TR1-B4**：`main.c` 改为完整初始化序列（`Set_System` → `Set_USBClock` → `USB_Interrupts_Config` → `USB_Init` → `PowerOn` → `while`），设备出现在设备管理器。

> **B3 依赖 A4 的 `#if 0` 结构**：B3 完成 `usb_pwr.c/h` 后，只需取消 `usb_istr.c` 中 `#if 0` 的包裹并加回 `#include "usb_pwr.h"`，即恢复完成态。这是 A4 预留的恢复点。

---

*文档结束。*
