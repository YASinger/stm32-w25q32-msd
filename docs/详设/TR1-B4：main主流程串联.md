# TR1-B4 详细设计：main 主流程串联

## 修改记录

| 版本 | 日期 | 修改内容 | 修改人 |
|---|---|---|---|
| V1.0 | 2026-07-19 | 初始版本 | Copilot |

---

## 文档信息

| 项目 | 内容 |
|---|---|
| 需求编号 | TR1-B4 |
| 需求描述 | `main.c` 主流程串联（Set_System → Set_USBClock → USB_Interrupts_Config → USB_Init → PowerOn → while） |
| 验收标准 | 烧录后流程跑通，设备出现在设备管理器 |
| 所属阶段 | TR1-B — 最小枚举通过（纵向切片） |
| 前置文档 | `项目框架设计.md`、`TR1框架设计.md` |
| 前置代码 | TR1-A 骨架全部完成；TR1-B1~B3 数据、机制、电源管理全部就位 |
| 完成态参照 | `tmp\last_project\src\main.c`（TR1 全部完成后的最终形态） |

---

## 1. 需求分解

TR1-B4 是 TR1-B 阶段的收尾，目标是把 `main.c` 的初始化序列串成完整流程。B1~B3 分别完成了数据（设备描述符）、机制（设备属性回调）、电源管理（PowerOn），B4 把它们和 A 阶段的骨架（时钟/GPIO/NVIC/中断）串成一条完整的初始化链。

**B4 是 TR1 阶段改动最小的任务**——只改 `main.c` 一个文件，加一行代码。但这一行代码有明确的语义：`while (bDeviceState != CONFIGURED)` 等待主机完成 SET_CONFIGURATION，设备真正进入配置状态。

**与完成态的关系**：`last_project` 的 `main.c` 比 B4 多两部分——`Get_SerialNum()`（C2）和 LED 控制（本项目无 LED）。B4 只做核心串联，C2 时再补 `Get_SerialNum()`。

---

## 2. 关键技术决策

### 2.1 `while (bDeviceState != CONFIGURED)` 的语义

**作用**：在 `PowerOn()` 后阻塞等待，直到主机完成 SET_CONFIGURATION，`bDeviceState` 变为 `CONFIGURED`。

**为什么需要这一行**：

没有这一行时（B3 的 `main.c`），`PowerOn()` 后直接进 `while(1)` 空循环。程序功能上没有问题——USB 中断驱动的枚举过程在后台进行，不需要主循环干预。但学习者无法直观感知"枚举是否完成"。

加了这一行后，程序在枚举完成前停在 `while` 循环，枚举完成后才进入 `while(1)`。这为后续调试提供了一个**明确的观测点**——在 `while(1)` 设断点，断点命中即说明枚举已完成。

**`bDeviceState` 的完整转换链**（A2~C3 协同）：

| 状态 | 触发者 | 设置位置 |
|---|---|---|
| UNCONNECTED | 上电初始 | `usb_pwr.c` 定义初始值 |
| ATTACHED | `PowerOn()` | `usb_pwr.c`（B3） |
| ATTACHED | `MASS_Reset()`（主机 RESET） | `usb_prop.c`（B2/B3） |
| ADDRESSED | SET_ADDRESS 完成 | USB 库内部（`usb_core.c`） |
| CONFIGURED | SET_CONFIGURATION 完成 | `Mass_Storage_SetConfiguration()`（C3） |

**关键依赖**：`bDeviceState = CONFIGURED` 的赋值在 `Mass_Storage_SetConfiguration()` 中（C3 实现）。B4 阶段 C3 尚未实现，`Mass_Storage_SetConfiguration()` 是 B2 的空函数，`bDeviceState` 永远不会变为 `CONFIGURED`。

**这意味着 B4 的 `while (bDeviceState != CONFIGURED)` 会永远阻塞**——程序停在循环里，不会进入 `while(1)`。

**处理**：这是 B4 的**预期行为**。B4 的验收标准是"烧录后流程跑通，设备出现在设备管理器"——设备管理器出现设备在 B2 就已实现（`MASS_Reset()` 配置 EP0/EP1/EP2 后主机即可识别），不依赖 `while` 循环是否退出。`while (bDeviceState != CONFIGURED)` 的阻塞不影响设备管理器的显示。

**验证方式**：烧录后设备管理器仍出现设备（与 B2/B3 相同），即 B4 验收通过。`while` 循环的阻塞是 C3 要解决的事——C3 实现 `Mass_Storage_SetConfiguration()` 后，`bDeviceState` 变为 `CONFIGURED`，循环退出。

### 2.2 与完成态的裁剪关系

| 完成态内容 | B4 是否保留 | 恢复时机 |
|---|---|---|
| `Set_System()` → `Set_USBClock()` → `USB_Interrupts_Config()` | ✅ 保留（A2 已有） | — |
| `USB_Init()` | ✅ 保留（B2 已有） | — |
| `PowerOn()` | ✅ 保留（B3 已有） | — |
| `while (bDeviceState != CONFIGURED);` | ✅ **B4 新增** | — |
| `while (1) {}` | ✅ 保留 | — |
| `Get_SerialNum()` | ❌ 删除 | C2 |
| LED 初始化/点亮 | ❌ 删除 | 本项目无 LED |

### 2.3 `while` 循环的优化

`while (bDeviceState != CONFIGURED);` 是一个忙等待循环，CPU 空转。在 TR1 学习阶段这是可接受的——简单直接，不引入额外复杂度。TR2 可改为中断驱动或低功耗等待。

---

## 3. 涉及组件

```
┌─────────────────────────────────────────────────┐
│  main.c — 完整初始化序列                          │
│                                                  │
│  [A2] Set_System()          GPIO + 时钟          │
│  [A2] Set_USBClock()        USB 48MHz            │
│  [A2] USB_Interrupts_Config() NVIC               │
│  [B2] USB_Init()            设备属性注册          │
│  [B3] PowerOn()             D+ 上拉 + CNTR 配置  │
│  [B4] while (bDeviceState != CONFIGURED)         │
│       ↓ 枚举完成 (C3 后退出)                      │
│  [B4] while (1) {}          主循环               │
└─────────────────────────────────────────────────┘
```

---

## 4. 文件清单

### 4.1 修改文件

| 文件 | 修改内容 |
|---|---|
| `src/main.c` | `PowerOn()` 后加 `while (bDeviceState != CONFIGURED);` |

### 4.2 新建文件

无。

### 4.3 工程分组

无变化。

---

## 5. 接口设计

### 5.1 `src/main.c`

```c
#include "stm32f10x.h"
#include "hw_config.h"
#include "usb_lib.h"
#include "usb_pwr.h"

int main(void)
{
  Set_System();
  Set_USBClock();
  USB_Interrupts_Config();
  USB_Init();
  PowerOn();

  while (bDeviceState != CONFIGURED);  /* 等待枚举完成 (C3 后退出) */

  while (1)
  {
  }
}
```

**变更说明**：在 `PowerOn()` 后加一行 `while (bDeviceState != CONFIGURED);`。这是 B4 的唯一改动。

**与完成态的差异**：
- 删除 `Get_SerialNum()`（C2 恢复）
- 删除 LED 初始化/点亮（本项目无 LED）

---

## 6. 实现要点与风险

### 6.1 `while` 循环永远阻塞（预期行为）

**问题**：C3 的 `Mass_Storage_SetConfiguration()` 未实现，`bDeviceState` 不会变为 `CONFIGURED`，`while` 循环永远阻塞。

**分析**：
- 设备管理器出现设备不依赖 `while` 循环退出——B2 的 `MASS_Reset()` 配置 EP0/EP1/EP2 后，主机即可识别设备
- `while` 循环的阻塞只是让程序停在某个位置，不影响 USB 中断驱动的枚举过程
- C3 实现 `Mass_Storage_SetConfiguration()` 后，`bDeviceState = CONFIGURED`，循环退出

**结论**：B4 的验收不依赖 `while` 循环退出。设备管理器出现设备即验收通过。

### 6.2 B4 的可观测性

B4 与 B3 的 USBTreeView 表现**完全一致**——`while` 循环是内部行为，不影响主机端的枚举过程。设备管理器仍出现设备，Problem Code 仍是 43（C1 才消失）。

B4 的可观测差异需要调试器：在 `while(1)` 设断点，断点不命中（因为 `while (bDeviceState != CONFIGURED)` 阻塞）。C3 后断点命中，证明枚举完成。

### 6.3 `bDeviceState` 的 `__IO` 属性

`bDeviceState` 声明为 `__IO uint32_t`（`usb_pwr.h`），`__IO` 是 `volatile` 的宏定义。这确保 `while` 循环每次迭代都从内存重新读取 `bDeviceState`，而不是用寄存器缓存的旧值。若缺少 `volatile`，编译器可能优化掉循环，导致永远阻塞或永远不阻塞。

`usb_pwr.h` 已正确使用 `__IO`，无需修改。

---

## 7. 验证流程

### 7.1 编译验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | Keil Build (F7) | 0 Error(s), 0 Warning(s) | 待验证 |

### 7.2 USBTreeView 验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 烧录，插 USB | 设备管理器仍出现设备 | **B4 核心验收** |
| 2 | USBTreeView 查看 | 与 B3 表现一致（设备描述符完整，Problem Code 43） | 待验证 |

### 7.3 断点验证（可选，需调试器）

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 断点设在 `while (bDeviceState != CONFIGURED)` | 烧录后命中（循环内） | 待验证 |
| 2 | 查看 `bDeviceState` 值 | = ATTACHED (1) 或 ADDRESSED (4)，不等于 CONFIGURED (5) | 待验证 |
| 3 | 断点设在 `while(1)` | 不命中（被 `while` 阻塞） | 预期行为 |

---

## 8. 常见问题排查

### 8.1 编译报错：identifier "CONFIGURED" is undefined

**原因**：`main.c` 未 `#include "usb_pwr.h"`，`CONFIGURED` 枚举值来自 `usb_pwr.h` 的 `DEVICE_STATE`。

**修复**：确认 `main.c` 第 4 行有 `#include "usb_pwr.h"`（B3 已加）。

### 8.2 设备管理器不出现设备

**原因**：`while` 循环阻塞导致 `PowerOn()` 未执行？不可能——`while` 在 `PowerOn()` 之后。

**排查**：若设备管理器不出现设备，问题不在 B4 的 `while` 循环，而在之前的某个环节（A2 硬件初始化/B2 USB_Init/B3 PowerOn）。回退到 B3 的 `main.c` 验证。

### 8.3 设备管理器出现设备但立刻消失

**原因**：`while` 循环阻塞导致看门狗复位？STM32F103C8 默认无独立看门狗，除非显式启用。

**排查**：确认未启用 IWDG/WWDG。若启用，`while` 循环内需喂狗。

---

## 9. 与下一步的衔接

TR1-B4 完成后，TR1-B 最小枚举阶段全部结束。设备管理器出现设备（带黄色感叹号），主机可读取设备描述符。

下一步进入 TR1-C 逐层补全：

- **TR1-C1**：`usb_desc.c/h` 补入 `MASS_ConfigDescriptor`（32B），`usb_prop.c` 补入 `MASS_GetConfigDescriptor` 和 `Config_Descriptor`。黄色感叹号消失，USBTreeView 显示 Mass Storage Class + 2 Bulk EP。
- **TR1-C2**：`usb_desc.c/h` 补入 4 组字符串描述符，`hw_config.c` 实现 `Get_SerialNum()`，`main.c` 加 `Get_SerialNum()` 调用。USBTreeView 显示厂商/产品/序列号。
- **TR1-C3**：`usb_prop.c` 实现 `Mass_Storage_SetConfiguration()`（`bDeviceState = CONFIGURED` + 清 EP1/EP2 DTOG）。`while (bDeviceState != CONFIGURED)` 退出，B4 的阻塞解除。

> **B4 是 TR1-B 的收尾，也是 TR1-C 的前置**：B4 的 `while` 循环等 C3 来解除，C1/C2/C3 每完成一个，USBTreeView 就有可观测差异。TR1-C 是"每次插 USB 都有新变化"的阶段。

---

*文档结束。*
