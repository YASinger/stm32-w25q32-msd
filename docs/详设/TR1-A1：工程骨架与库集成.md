# TR1-A1 详细设计：工程骨架与库集成

## 修改记录

| 版本 | 日期 | 修改内容 | 修改人 |
|---|---|---|---|
| V1.0 | 2026-07-17 | 初始版本 | Copilot |
| V1.1 | 2026-07-18 | 补充编译验证发现的三处库硬依赖（`hw_config.h`/`IMR_MSK`/`usb_globals.c`）；验证状态更新为已通过 | Copilot |

---

## 文档信息

| 项目 | 内容 |
|---|---|
| 需求编号 | TR1-A1 |
| 需求描述 | 工程骨架 + 标准外设库 + USB 库集成，`main.c` 空壳可编译 |
| 验收标准 | Keil 编译通过、链接通过，0 Error 0 Warning |
| 所属阶段 | TR1-A — USB 栈骨架可编译运行 |
| 前置文档 | `项目框架设计.md`、`TR1框架设计.md` |

---

## 1. 需求分解

TR1-A1 是整个项目的第一个开发任务，目标是**搭一个能编译通过的空壳工程**。这个空壳包含：

- CMSIS 启动文件与系统初始化（`SystemInit` 配置 72MHz 时钟）
- STM32 标准外设库 V3.5.0 全量集成
- STM32 USB-FS-Device 库 V4.1.0 全量集成
- `main.c` 空的 `main()` 函数
- `stm32f10x_it.c/h` 中断服务模板（只含 Cortex-M3 异常处理，无外设中断）
- `stm32f10x_conf.h` 库配置文件（include 全部外设头文件）

**本任务不写任何 USB 业务代码**。USB 库的 `.c` 文件加入工程但不会被 `main.c` 调用——它们只是"已就位"状态，后续 TR1-A2~A4 会逐步接线。

> **为什么要把 USB 库全量加入工程？**
> USB 库内部有交叉引用（`usb_core.c` 依赖 `usb_regs.c`，`usb_init.c` 依赖 `usb_int.c` 等），部分函数互相 call。如果只加一半，链接器会报 undefined symbol。所以骨架阶段就把 6 个 `.c` 全部加入，确保后续每一步都能直接调用。

### 1.1 编译验证发现的库硬依赖（V1.1 补充）

实际 Keil 编译验证表明，"只加库 `.c` + `usb_conf.h` 占位"不足以通过编译链接。ST USB 库存在三处与应用层的硬耦合，A1 必须额外提供占位：

| 耦合点 | 库引用处 | 后果 | A1 修复 |
|---|---|---|---|
| `usb_lib.h:44` `#include "hw_config.h"` | 6 个 USB 库 `.c` 全部 | 6 个编译错误 | 创建 `inc/hw_config.h` 占位 |
| `usb_regs.h` 使用 `__IO`/`uint16_t` 却不自包含 | `usb_regs.h:687` 等 | 180 个编译错误 | `hw_config.h` 内 `#include "stm32f10x.h"` |
| `usb_sil.c:73` 引用 `IMR_MSK` | `USB_SIL_Init()` | 1 个编译错误 | `usb_conf.h` 补 `IMR_MSK` 定义 |
| `usb_core.c`/`usb_init.c`/`usb_int.c` 引用 6 个全局符号 | 链接阶段 | 6 个链接错误 | 新建 `src/usb_globals.c` weak 占位 |

**关键认知**：`hw_config.h` 不仅是 A2 的硬件配置文件，更是 USB 库的类型依赖入口——`usb_lib.h` 的第一个 `#include` 就是它，所有 CMSIS 类型（`__IO`、`uint16_t` 等）经此引入。这意味着 A2 编写 `hw_config.c/h` 时，头文件部分必须在 A1 就确立的框架上扩展，不能推翻重写。

---

## 2. 涉及组件

```
┌─────────────────────────────────────────────────┐
│              Keil MDK 编译器                      │
│   目标: 编译 + 链接通过, 生成 .axf                │
└────────────────────┬────────────────────────────┘
                     │
╔════════════════════╧═════════════════════════════╗
║  project.uvprojx (工程文件)                       ║
║  ├── Target: STM32F103C8, Cortex-M3              ║
║  ├── Define: USE_STDPERIPH_DRIVER                 ║
║  └── IncludePath: Start / inc / StdPeriph / USB   ║
╠═══════════════════════════════════════════════════╣
║  Group: src                                       ║
║  ├── main.c                (空 main)              ║
║  ├── stm32f10x_it.c        (异常处理模板)         ║
║  ├── usb_globals.c         (库符号 weak 占位)     ║
║  ├── stm32f10x_it.h                               ║
║  ├── stm32f10x_conf.h      (库配置)               ║
║  ├── usb_conf.h            (占位 + IMR_MSK)       ║
║  └── hw_config.h           (占位 + 类型入口)      ║
╠═══════════════════════════════════════════════════╣
║  Group: Start                                      ║
║  ├── startup_stm32f10x_md.s  (启动文件)           ║
║  ├── core_cm3.c / core_cm3.h                     ║
║  ├── stm32f10x.h                                 ║
║  └── system_stm32f10x.c / .h  (SystemInit)        ║
╠═══════════════════════════════════════════════════╣
║  Group: StdPeriph  (23 个外设, 全量)              ║
║  Group: USB-FS-Device (6 个 .c, 全量)            ║
╚═══════════════════════════════════════════════════╝
```

---

## 3. 文件清单

### 3.1 已存在文件（本任务不修改内容，只确认在工程中）

| 文件 | 说明 |
|---|---|
| `Start/startup_stm32f10x_md.s` | STM32F103C8 属于中容量产品（md），启动文件选 `startup_stm32f10x_md.s`。含向量表、`Reset_Handler`、`SystemInit` 调用、跳转 `__main` |
| `Start/core_cm3.c` / `core_cm3.h` | CMSIS Cortex-M3 核心支持 |
| `Start/stm32f10x.h` | 设备头文件，定义寄存器地址、中断号、`HSE_VALUE` 等 |
| `Start/system_stm32f10x.c` / `system_stm32f10x.h` | `SystemInit()` 配置 HCLK=72MHz（HSE 8MHz × PLL9）。由启动文件在进入 `main` 前调用 |
| `src/main.c` | 空的 `main()`，只 `return 0` |
| `src/stm32f10x_it.c` | Cortex-M3 异常处理模板（NMI/HardFault/SVC/...），无外设中断 |
| `inc/stm32f10x_it.h` | 异常处理函数声明 |
| `inc/stm32f10x_conf.h` | 库配置，include 全部 23 个外设头文件 + `misc.h`，定义 `assert_param` 宏 |

### 3.2 本任务新建文件

| 文件 | 说明 |
|---|---|
| `inc/usb_conf.h` | **USB 配置占位文件**。USB 库的 `usb_type.h:44` 有 `#include "usb_conf.h"`，而 `usb_type.h` 被 USB 库几乎所有文件引用。若没有此文件，编译会报 `cannot open source input file "usb_conf.h"`。占位版含 `#ifndef`/`#define`/`#endif` 守卫和注释。正式的端点数、PMA 地址、回调绑定等内容在 TR1-A3 阶段填充。 |
| `inc/usb_conf.h` 的 `IMR_MSK` | **中断屏蔽宏（V1.1 补充）**。`usb_sil.c:73` 的 `USB_SIL_Init()` 硬性引用 `IMR_MSK` 设置 CNTR 中断屏蔽寄存器。掩码值 `(CNTR_CTRM \| CNTR_WKUPM \| CNTR_SUSPM \| CNTR_ERRM \| CNTR_SOFM \| CNTR_ESOFM \| CNTR_RESETM)`，遵循 §6.2 禁用挂起模式约束（包含 `CNTR_SUSPM` 以软件方式处理挂起）。 |
| `inc/hw_config.h` | **硬件配置占位文件（V1.1 补充）**。ST USB 库 `usb_lib.h:44` 硬性 `#include "hw_config.h"`，缺失导致 6 个 USB 库 `.c` 编译失败。文件内含 `#include "stm32f10x.h"`——因为 `usb_regs.h` 直接使用 `__IO`/`uint16_t` 等 CMSIS 类型却不自包含，`hw_config.h` 经 `usb_lib.h` 成为 USB 库的类型依赖入口。正式的 `Set_System()` 等接口在 TR1-A2 填充。 |
| `src/usb_globals.c` | **USB 库全局符号 weak 占位（V1.1 补充）**。USB 库 `usb_core.c`/`usb_init.c`/`usb_int.c` 在链接阶段引用 6 个全局符号（`wIstr`/`pEpInt_IN[7]`/`pEpInt_OUT[7]`/`Device_Table`/`Device_Property`/`User_Standard_Requests`），它们按设计分别属 TR1-A4（`usb_istr.c`）和 TR1-B2（`usb_prop.c`）。但 A1 验收要求"链接通过"，故用 `__attribute__((weak))` 提供占位。A4/B2 正式实现时无需删除本文件——链接器自动选择强定义覆盖。 |

### 3.2 工程分组结构（`project.uvprojx`）

| Group | 文件 | 文件类型 |
|---|---|---|
| **src** | `main.c` | C 源 |
| | `stm32f10x_it.c` | C 源 |
| | `usb_globals.c` | C 源（weak 占位） |
| | `stm32f10x_it.h` | 头文件 |
| | `stm32f10x_conf.h` | 头文件 |
| | `usb_conf.h` | 头文件（占位 + IMR_MSK） |
| | `hw_config.h` | 头文件（占位 + 类型入口） |
| **Start** | `startup_stm32f10x_md.s` | 汇编 |
| | `core_cm3.c` | C 源 |
| | `core_cm3.h` | 头文件 |
| | `stm32f10x.h` | 头文件 |
| | `system_stm32f10x.c` | C 源 |
| | `system_stm32f10x.h` | 头文件 |
| **StdPeriph** | `misc.c/h` + 22 个外设 `.c/.h` | C 源 + 头文件 |
| **USB-FS-Device** | `usb_core.c/h`、`usb_init.c/h`、`usb_int.c/h`、`usb_mem.c/h`、`usb_regs.c/h`、`usb_sil.c/h`、`usb_def.h`、`usb_lib.h`、`usb_type.h` | C 源 + 头文件 |

---

## 4. 编译选项配置

### 4.1 预处理宏

| 宏 | 值 | 作用 |
|---|---|---|
| `USE_STDPERIPH_DRIVER` | 定义 | 在 `stm32f10x.h` 中启用 `#include "stm32f10x_conf.h"`，从而引入全部外设头文件 |

> **不加 `USE_STDPERIPH_DRIVER` 的后果**：`stm32f10x.h` 不会 include `stm32f10x_conf.h`，编译器找不到 `GPIO_Init` 等函数声明，报大量 undefined。

### 4.2 Include 路径

```
.\Start;.\inc;.\Library\STM32F10x_StdPeriph_Driver\inc;.\Library\STM32_USB-FS-Device_Driver\inc
```

| 路径 | 提供 |
|---|---|
| `.\Start` | `stm32f10x.h`、`core_cm3.h`、`system_stm32f10x.h` |
| `.\inc` | `stm32f10x_conf.h`、`stm32f10x_it.h` |
| `.\Library\STM32F10x_StdPeriph_Driver\inc` | 23 个外设头文件 + `misc.h` |
| `.\Library\STM32_USB-FS-Device_Driver\inc` | 9 个 USB 库头文件 |

### 4.3 目标设备

| 配置 | 值 |
|---|---|
| Device | STM32F103C8 |
| Cpu | `IRAM(0x20000000,0x5000) IROM(0x08000000,0x10000)` |
| IRAM | 0x20000000, 20KB（0x5000） |
| IROM | 0x08000000, 64KB（0x10000） |
| CPUTYPE | Cortex-M3 |
| CLOCK | 12MHz（调试器参考时钟，不影响实际 PLL） |

### 4.4 编译器选项

| 选项 | 值 | 说明 |
|---|---|---|
| Optimization | Level 1 (`-O1`) | 默认，平衡速度与可调试性 |
| C99 Mode | 启用 | 允许 `//` 注释和混合声明 |
| One ELF Section per Function | 启用 | 链接器可丢弃未调用函数，减小固件体积 |
| Interwork | 启用 | ARM/Thumb 互转（Cortex-M3 默认） |

### 4.5 链接器选项

| 选项 | 值 |
|---|---|
| Use Memory from Target Dialog | 启用（`umfTarg=1`） |
| Text Address Range | 0x08000000 |
| Data Address Range | 0x20000000 |
| ScatterFile | 无（使用 Target 对话框的内存配置） |

---

## 5. 关键文件说明

### 5.1 `src/main.c`

```c
#include "stm32f10x.h"                  // Device header



int main(void) {
	return 0;
}
```

**说明**：
- `#include "stm32f10x.h"` 会因 `USE_STDPERIPH_DRIVER` 宏触发 `#include "stm32f10x_conf.h"`，从而引入全部外设头文件。这是后续所有代码的基础。
- `return 0` 从 `main` 返回后进入 `__mian` 的半主机模式（semihosting），在不连接调试器时可能进入 HardFault。TR1-A1 阶段只验证编译，不关心运行行为。后续 TR1-A2 会把 `main` 改成 `while(1)` 死循环。

### 5.2 `inc/stm32f10x_conf.h`

已 include 全部 23 个外设头文件 + `misc.h`。`assert_param` 宏未启用（`USE_FULL_ASSERT` 未定义），`assert_param` 展开为空——这是骨架阶段的合理选择，避免每条 assert 在运行时拖慢速度。

> 后续调试 USB 时若需要参数检查，可临时 `#define USE_FULL_ASSERT 1` 并在 `stm32f10x_it.c` 或 `main.c` 实现 `assert_failed()`。

### 5.3.1 `inc/hw_config.h`（V1.1 补充）

```c
#ifndef __HW_CONFIG_H
#define __HW_CONFIG_H

#include "stm32f10x.h"
/* 正式接口由 TR1-A2 填充 */

#endif
```

**说明**：
- ST USB 库 `usb_lib.h:44` 硬性 `#include "hw_config.h"`，缺失则 6 个 USB 库 `.c` 全部编译失败。
- `#include "stm32f10x.h"` 是必须的——`usb_regs.h` 直接使用 `__IO`、`uint16_t` 等 CMSIS 类型却不自包含。`usb_lib.h` 的 include 顺序是 `hw_config.h` → `usb_type.h` → `usb_regs.h` → ...，所以 `hw_config.h` 是 USB 库的类型依赖入口。
- 正式的 `Set_System()`/`Set_USBClock()`/`USB_Interrupts_Config()`/`USB_Cable_Config()` 接口在 TR1-A2 填充。

### 5.3.2 `inc/usb_conf.h` 的 `IMR_MSK`（V1.1 补充）

```c
#define IMR_MSK (CNTR_CTRM  | CNTR_WKUPM | CNTR_SUSPM | CNTR_ERRM  | CNTR_SOFM \
                 | CNTR_ESOFM | CNTR_RESETM )
```

**说明**：
- `usb_sil.c:73` 的 `USB_SIL_Init()` 直接引用 `IMR_MSK` 设置 CNTR 中断屏蔽寄存器。
- 掩码包含 `CNTR_SUSPM`：遵循 §6.2 禁用挂起模式约束——收到 SUSP 中断后由软件处理状态（设 `bDeviceState`），而非进入 STOP 模式。
- 各 `CNTR_xxxM` 位定义来自 `usb_regs.h:147-155`，经 `usb_lib.h` 间接包含。
- 正式的端点数、PMA 地址、回调绑定等内容仍留待 TR1-A3 填充。

### 5.3.3 `src/usb_globals.c`（V1.1 补充）

```c
__IO uint16_t wIstr __attribute__((weak)) = 0;
void (*pEpInt_IN[7])(void)  __attribute__((weak)) = {0};
void (*pEpInt_OUT[7])(void) __attribute__((weak)) = {0};
DEVICE Device_Table __attribute__((weak)) = { 0, 0 };
DEVICE_PROP Device_Property __attribute__((weak)) = { 0 };
USER_STANDARD_REQUESTS User_Standard_Requests __attribute__((weak)) = { 0 };
```

**说明**：
- USB 库在链接阶段引用这 6 个全局符号，它们按设计分别属 TR1-A4（`wIstr`/`pEpInt_IN`/`pEpInt_OUT` 定义于 `usb_istr.c`）和 TR1-B2（`Device_Table`/`Device_Property`/`User_Standard_Requests` 定义于 `usb_prop.c`）。
- A1 验收要求"链接通过"，故用 `__attribute__((weak))` 提供占位。
- `weak` 属性确保 A4/B2 正式实现时无需删除本文件——链接器自动选择强定义覆盖弱定义。
- 占位值为零/NULL：A1 阶段即使 USB 中断意外触发也不会执行野指针（符合"PC 不识别设备"的 A1 特征）。

### 5.3 `src/stm32f10x_it.c`

只包含 Cortex-M3 内核异常处理（NMI / HardFault / SVC / PendSV / SysTick），无外设中断。`HardFault_Handler` 是 `while(1)` 死循环——调试时若命中说明出了严重错误。

> **后续位置**：TR1-A4 会在本文件或 `usb_istr.c` 中添加 `USB_LP_CAN1_RX0_IRQHandler`。根据之前项目的经验，USB 中断入口放在 `usb_istr.c` 而非 `stm32f10x_it.c`，以保持标准模板文件不被污染。

### 5.4 `Start/system_stm32f10x.c`

`SystemInit()` 在启动文件中 `__main` 之前被调用，完成时钟配置：
- HSE 8MHz → PLL × 9 → SYSCLK 72MHz
- AHB = SYSCLK = 72MHz (HCLK)
- APB1 = HCLK / 2 = 36MHz (PCLK1)
- APB2 = HCLK = 72MHz (PCLK2)

> **USB 时钟来源**：USB 时钟需要 48MHz，由 PLLCLK ÷ 1.5 = 72 ÷ 1.5 = 48MHz 提供。这一步在 `Set_USBClock()` 中配置（TR1-A2），`SystemInit` 只负责系统时钟。

### 5.5 启动文件选择

STM32F103C8 是中容量产品（Flash 64~128KB），启动文件选 `startup_stm32f10x_md.s`。

| 启动文件 | 适用产品 | 中断向量数 |
|---|---|---|
| `startup_stm32f10x_ld.s` | 小容量 (Flash ≤ 32KB) | 43 |
| **`startup_stm32f10x_md.s`** | **中容量 (64~128KB)** | **43** |
| `startup_stm32f10x_hd.s` | 大容量 (256~512KB) | 60 |
| `startup_stm32f10x_cl.s` | 互联型 (STM32F105/107) | 68 |
| `startup_stm32f10x_xl.s` | 超大容量 (≥ 512KB) | 60 |
| `*_vl.s` | 超值型 (STM32F100) | — |

> **常见错误**：选错启动文件会导致中断向量表不匹配，USB 中断无法进入。STM32F103C8 必须用 `md` 版本。

---

## 6. 当前状态确认

以下文件/配置在 TR1-A1 任务开始前**已就位**，本任务不新增或修改任何文件内容：

| 检查项 | 状态 | 说明 |
|---|---|---|
| `src/main.c` | ✅ 已存在 | 空壳，`#include "stm32f10x.h"` + 空 `main` |
| `src/stm32f10x_it.c` | ✅ 已存在 | 内核异常模板 |
| `inc/stm32f10x_it.h` | ✅ 已存在 | 异常处理声明 |
| `inc/stm32f10x_conf.h` | ✅ 已存在 | include 全部外设头文件 |
| `inc/usb_conf.h` | ✅ 本任务创建 | 占位文件 + `IMR_MSK` 宏（满足 `usb_type.h` 与 `usb_sil.c` 依赖） |
| `inc/hw_config.h` | ✅ 本任务创建 | 占位文件 + `#include "stm32f10x.h"`（USB 库类型依赖入口） |
| `src/usb_globals.c` | ✅ 本任务创建 | 6 个 USB 库全局符号的 weak 占位 |
| `Start/` 全部文件 | ✅ 已存在 | 启动文件 + CMSIS + 系统初始化 |
| `Library/StdPeriph/` | ✅ 已存在 | 23 个外设 + misc |
| `Library/USB-FS-Device/` | ✅ 已存在 | 6 个 .c + 9 个 .h |
| `project.uvprojx` 分组 | ✅ 已配置 | 4 个 Group，全部文件已加入（含 `hw_config.h`、`usb_globals.c`） |
| `USE_STDPERIPH_DRIVER` 宏 | ✅ 已定义 | — |
| IncludePath | ✅ 已配置 | 4 条路径 |
| 设备选型 STM32F103C8 | ✅ 已配置 | IRAM 0x5000, IROM 0x10000 |
| Keil 编译验证 | ✅ 已通过 | 0 Error 0 Warning，`Objects/project.axf` 生成 |

---

## 7. 验证流程

### 7.1 编译验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | Keil 打开 `project.uvprojx` | 工程加载成功，无 "File not found" 报错 | ✅ 已通过 |
| 2 | Project → Build Target (F7) | Compiling... → 0 Error(s), 0 Warning(s) | ✅ 已通过 |
| 3 | 查看 Output 窗口 | 显示 `Program Size: Code=XX RO-data=XX RW-data=XX ZI-data=XX` | ✅ 已通过 |
| 4 | 查看 `.axf` 文件生成 | `Objects/project.axf` 存在 | ✅ 已通过 |

### 7.2 链接验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 检查未调用函数是否被丢弃 | 由于 OneElfS=1，未被 `main` 引用的外设函数应被链接器丢弃，Code Size 较小 | ✅ 已通过 |
| 2 | 检查 `Code` 段大小 | 预计 < 2KB（只有 `main` + 启动 + `SystemInit` + C 库初始化） | ✅ 已通过 |

### 7.3 烧录验证（可选）

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 连接 ST-Link / J-Link | Keil 识别调试器 | 待验证 |
| 2 | Flash → Download (F8) | 烧录成功 | 待验证 |
| 3 | Debug → Start/Stop (Ctrl+F5) | 进入调试模式，停在 `Reset_Handler` 或 `main` | 待验证 |
| 4 | F5 全速运行 | `main` 执行 `return 0` 后可能进入半主机 HardFault（属正常，无调试器时） | 待验证 |

> TR1-A1 的核心验收是**编译通过**，烧录运行不是必须的。运行行为（`main` 返回后发生什么）在 TR1-A2 会通过改为 `while(1)` 解决。
> **注意**：编译链接验证已于 2026-07-18 通过 Keil V5.06 实测，0 Error 0 Warning。烧录验证可在 A2 硬件初始化完成后一并验证。

---

## 8. 常见问题排查

### 8.1 编译报错：cannot open source input file "stm32f10x.h"

**原因**：IncludePath 缺少 `.\Start`。

**修复**：Options → C/C++ → Include Paths 添加 `.\Start`。

### 8.2 编译报错：cannot open source input file "usb_conf.h"

**原因**：USB 库的 `usb_type.h:44` 有 `#include "usb_conf.h"`，而 `usb_type.h` 被 USB 底层几乎所有文件引用。缺少 `usb_conf.h` 会导致 USB 库全部编译失败。

**修复**：确认 `inc/usb_conf.h` 存在（本任务创建的占位文件）。确认 `project.uvprojx` 的 `src` Group 已加入该文件。确认 IncludePath 包含 `.\inc`。

### 8.3 编译报错：undefined symbol GPIO_Init（或类似外设函数）

**原因**：未定义 `USE_STDPERIPH_DRIVER` 宏，或 `stm32f10x_conf.h` 没有 include 对应外设头文件。

**修复**：Options → C/C++ → Define 添加 `USE_STDPERIPH_DRIVER`。检查 `stm32f10x_conf.h` 中对应 `#include "stm32f10x_gpio.h"` 未被注释。

### 8.4 链接报错：undefined symbol USB_Init（或类似 USB 库函数）

**原因**：USB 库 `.c` 文件未加入工程。

**修复**：检查 `project.uvprojx` 的 `USB-FS-Device` Group 是否包含全部 6 个 `.c` 文件。

### 8.4.1 编译报错：cannot open source input file "hw_config.h"（V1.1 补充）

**原因**：ST USB 库 `usb_lib.h:44` 硬性 `#include "hw_config.h"`，而 `hw_config.h` 按 TR1 设计属 A2 任务，A1 未创建。

**修复**：创建 `inc/hw_config.h` 占位文件（仅头文件守卫 + 注释），并加入 uvprojx 的 src Group。正式内容由 TR1-A2 填充。

### 8.4.2 编译报错：identifier "__IO" is undefined（V1.1 补充）

**原因**：`usb_regs.h` 直接使用 `__IO`/`uint16_t` 等 CMSIS 类型却不自包含，依赖上游文件提供。`usb_lib.h` 的 include 顺序决定 `hw_config.h` 是类型依赖入口，若占位文件不含 `#include "stm32f10x.h"`，则 180 个编译错误连锁爆发。

**修复**：在 `inc/hw_config.h` 守卫内加入 `#include "stm32f10x.h"`。

### 8.4.3 编译报错：identifier "IMR_MSK" is undefined（V1.1 补充）

**原因**：`usb_sil.c:73` 的 `USB_SIL_Init()` 直接引用 `IMR_MSK` 设置 CNTR 中断屏蔽。该宏按设计属 TR1-A3 的 `usb_conf.h`，但 A1 编译已需要。

**修复**：在 `inc/usb_conf.h` 补入 `IMR_MSK` 定义，掩码值 `(CNTR_CTRM | CNTR_WKUPM | CNTR_SUSPM | CNTR_ERRM | CNTR_SOFM | CNTR_ESOFM | CNTR_RESETM)`。

### 8.4.4 链接报错：Undefined symbol Device_Property / Device_Table / User_Standard_Requests / pEpInt_IN / pEpInt_OUT / wIstr（V1.1 补充）

**原因**：USB 库 `usb_core.c`/`usb_init.c`/`usb_int.c` 在链接阶段引用 6 个全局符号，它们按设计分别属 TR1-A4（`usb_istr.c`）和 TR1-B2（`usb_prop.c`），但 A1 验收要求"链接通过"。

**修复**：新建 `src/usb_globals.c`，用 `__attribute__((weak))` 提供 6 个占位定义。A4/B2 正式实现时链接器自动选择强定义覆盖，无需删除本文件。

### 8.5 编译警告：#1361-D: no corresponding #endif

**原因**：`stm32f10x_conf.h` 的 `#ifndef __STM32F10x_CONF_H` 守卫不匹配。

**修复**：检查文件末尾是否有 `#endif`。

### 8.6 烧录后 HardFault

**原因**：`main` 执行 `return 0` 后进入半主机模式，无调试器时触发 HardFault。

**修复**：这是 TR1-A1 的预期行为，不修。TR1-A2 会把 `main` 改为 `while(1)`。

---

## 9. 与下一步的衔接

TR1-A1 完成后，工程骨架就位，USB 库已集成但未使用。下一步：

- **TR1-A2**：在 `inc/hw_config.h` 占位基础上扩展接口声明，新建 `src/hw_config.c` 实现 `Set_System()`（HCLK 72MHz / USB 48MHz / PA11 PA12 / NVIC 组 2 / USBWakeUp 禁用）、`Set_USBClock()`、`USB_Interrupts_Config()`、`USB_Cable_Config()`。`main.c` 改为 `while(1)` 死循环。
- **TR1-A3**：在 `inc/usb_conf.h` 现有 `IMR_MSK` 基础上补全端点数（`EP_NUM`）、PMA 缓冲区地址（`BTABLE_ADDRESS`/`ENDP0_RXADDR` 等）、端点回调宏（全指向 `NOP_Process`）。
- **TR1-A4**：新建 `src/usb_istr.c`，定义 `wIstr`/`pEpInt_IN`/`pEpInt_OUT`（强定义覆盖 `usb_globals.c` 的 weak 占位），实现 USB 中断入口 `USB_LP_CAN1_RX0_IRQHandler` 和 `USB_Istr()` ISTR 事件分发。
- **TR1-B2**：新建 `src/usb_prop.c`，定义 `Device_Table`/`Device_Property`/`User_Standard_Requests`（强定义覆盖 `usb_globals.c` 的 weak 占位），实现 `MASS_Reset` 等回调。

> **关于 `usb_globals.c`**：当 A4 和 B2 都完成后，`usb_globals.c` 中的 6 个 weak 占位已全部被强定义覆盖，可从工程中移除该文件。也可保留——weak 符号被覆盖时不会产生链接冲突，但会浪费少量 Flash（零初始化的占位数据）。建议 A4/B2 完成后移除。

这三步完成后，插入 USB 就能在 `USB_Istr()` 断点命中——届时骨架阶段全部完成，进入 TR1-B 最小枚举。

---

*文档结束。*
