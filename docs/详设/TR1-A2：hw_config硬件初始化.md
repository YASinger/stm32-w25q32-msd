# TR1-A2 详细设计：hw_config 硬件初始化

## 修改记录

| 版本 | 日期 | 修改内容 | 修改人 |
|---|---|---|---|
| V1.0 | 2026-07-18 | 初始版本 | Copilot |

---

## 文档信息

| 项目 | 内容 |
|---|---|
| 需求编号 | TR1-A2 |
| 需求描述 | `hw_config.c/h` 硬件初始化（HCLK 72MHz / USB 48MHz / PA11 PA12 / NVIC） |
| 验收标准 | 断点能跑到 `while(1)`，PA12 已切为 AF_PP |
| 所属阶段 | TR1-A — USB 栈骨架可编译运行 |
| 前置文档 | `项目框架设计.md`、`TR1框架设计.md`、`TR1-A1：工程骨架与库集成.md` |
| 前置代码 | TR1-A1 已创建 `inc/hw_config.h` 占位（含 `#include "stm32f10x.h"`） |

---

## 1. 需求分解

TR1-A2 的目标是让 STM32 的 USB 外设**物理可用**。在 A1 的空壳工程基础上，本任务实现 `hw_config.c` 硬件初始化，让断点能跑到 `while(1)`，且 PA12 已配置为 USB D+ 引脚的复用推挽模式。

本任务需要实现 5 个函数：

| 函数 | 职责 | 被调用时机 |
|---|---|---|
| `Set_System()` | GPIO 时钟使能 + PA11/PA12 配置为 AF_PP | `main` 第一个调用 |
| `Set_USBClock()` | USB 48MHz 时钟源选择 + USB 外设时钟使能 | `Set_System` 之后 |
| `USB_Interrupts_Config()` | NVIC 优先级组 2 + USB_LP_CAN1_RX0 中断使能 | `Set_USBClock` 之后 |
| `USB_Cable_Config()` | 软件重连：PA12 在 Out_PP ↔ AF_PP 间切换 | TR1-C5 使用（本任务仅实现接口） |
| `Enter_LowPowerMode()` / `Leave_LowPowerMode()` | 低功耗进出（TR1 禁用挂起，仅提供空实现满足链接） | `usb_pwr.c`（B3）调用 |

**与参考例程的差异**：ST 官方 `hw_config.c` 包含多芯片 `#ifdef`、EVAL 板 LED 控制、SDIO、EXTI 唤醒等与本项目无关的代码。本项目按 `TR1框架设计.md` §6.1~6.5 的约束**裁剪重写**，只保留 STM32F103C8 + 无 PMOS 上拉开关 + 禁用挂起 所需的最小实现。

---

## 2. 关键技术决策

以下决策均来自 `TR1框架设计.md` §6 的明确约束，实现时不可偏离。

### 2.1 D+ 上拉方案（§6.1）

**本板硬件**：无 PMOS 开关，R10 硬接到 3.3V（1.5kΩ 固定上拉）。这意味着**不能通过外部电路断开 D+ 上拉**，软件重连必须通过 PA12 本身实现。

**实现方式**：PA12 在两种模式间切换：

| 状态 | PA12 模式 | 效果 |
|---|---|---|
| 断开 | `GPIO_Mode_Out_PP`，输出低电平 | 主机看到 D+ 被拉低，认为设备未连接 |
| 连接 | `GPIO_Mode_AF_PP`，交由 USB 外设控制 | USB 外设驱动 D+，主机认为设备已连接 |

**时序约束**：`Set_System()` 阶段先把 PA12 设为 Out_PP 输出低（让主机认为设备未连接），待全部初始化完成后，`PowerOn()`（B3 实现）再把 PA12 切为 AF_PP。但 A2 的验收标准要求"断点跑到 `while(1)` 时 PA12 已切为 AF_PP"——这意味着 A2 的 `main.c` 必须调用一个**临时的 PA12 切换**，以便示波器/万用表能测量到。

**A2 的处理**：A2 在 `main.c` 中调用 `Set_System()` + `Set_USBClock()` + `USB_Interrupts_Config()` 后，直接调用 `USB_Cable_Config(ENABLE)` 把 PA12 切为 AF_PP，进入 `while(1)`。这样满足验收"PA12 已切为 AF_PP"。B3 的 `PowerOn()` 会接管这一职责，届时 `main.c` 改为调用 `PowerOn()`。

### 2.2 禁用挂起模式（§6.2）

- `fSuspendEnabled = FALSE`（在 B3 的 `usb_pwr.c` 中设置，A2 不涉及）
- **USBWakeUp 中断 `DISABLE`**（A2 的 `USB_Interrupts_Config()` 中明确禁用）
- 原因：主机在发 RESET 前的总线空闲期会触发 SUSP 中断，若 `fSuspendEnabled` 为 `TRUE` 则 MCU 进入 STOP 模式且无法唤醒（USBWakeUp 中断未配置），导致枚举彻底失败。

### 2.3 USB 时钟来源（§6.4）

- HCLK = 72MHz（HSE 8MHz × 9 PLL，由 `SystemInit()` 在启动文件中配置，A2 不做）
- **USB 时钟 = PLLCLK ÷ 1.5 = 48MHz ± 0.25%**，必须精确
- 实现：`RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_1Div5)` + `RCC_APB1PeriphClockCmd(RCC_APB1Periph_USB, ENABLE)`

### 2.4 中断配置（§6.5）

| 中断 | 优先级组 | 抢占优先级 | 响应优先级 |
|---|---|---|---|
| USB_LP_CAN1_RX0_IRQn | 2 | 0 | 0 |
| USBWakeUp_IRQn | — | — | **DISABLE**（TR1 不使用） |

> **注意**：参考例程的 `USB_Interrupts_Config()` 把 USB_LP 的抢占优先级设为 2，而文档 §6.5 要求 0。本项目按文档执行，抢占优先级设为 0（最高），确保 USB 中断能及时响应。响应优先级 0。

### 2.5 端点分配（§6.6，A2 仅 GPIO 层）

| 引脚 | 功能 | 模式 |
|---|---|---|
| PA11 | USB DM | AF_PP（复用推挽） |
| PA12 | USB DP | Out_PP（初始）→ AF_PP（PowerOn 后） |

---

## 3. 涉及组件

```
┌─────────────────────────────────────────────────┐
│              main.c (TR1-B4 串联)                │
│   A2 阶段: Set_System → Set_USBClock →            │
│            USB_Interrupts_Config →               │
│            USB_Cable_Config(ENABLE) → while(1)    │
└────────────────────┬────────────────────────────┘
                     │
╔════════════════════╧═════════════════════════════╗
║  hwinit (硬件驱动层)                              ║
║  ┌───────────────────────────────────────────┐  ║
║  │ hw_config.c / hw_config.h                 │  ║
║  │  ├─ Set_System()         GPIO 时钟+PA11/12│  ║
║  │  ├─ Set_USBClock()       USB 48MHz        │  ║
║  │  ├─ USB_Interrupts_Config() NVIC          │  ║
║  │  ├─ USB_Cable_Config()   软件重连接口      │  ║
║  │  └─ Enter/Leave_LowPowerMode() 空实现      │  ║
║  └───────────────────────────────────────────┘  ║
╠══════════════════════════════════════════════════╣
║  基础库                                          ║
║  ┌──────────────┐  ┌──────────────────────────┐ ║
║  │ StdPeriph    │  │ CMSIS (core_cm3)         │ ║
║  │ GPIO/RCC/NVIC│  │ SystemInit (启动文件)     │ ║
║  └──────────────┘  └──────────────────────────┘ ║
╚══════════════════════════════════════════════════╝
```

**依赖关系**：
- `hw_config.c` 依赖 StdPeriph 库的 `GPIO_InitTypeDef`/`RCC_APB2PeriphClockCmd`/`NVIC_InitTypeDef` 等
- `hw_config.c` 依赖 CMSIS 的 `SystemInit()`（已由启动文件在 `main` 前调用，A2 不重做）
- `hw_config.h` 已包含 `stm32f10x.h`（A1 建立），`hw_config.c` 只需 `#include "hw_config.h"`

---

## 4. 文件清单

### 4.1 修改文件

| 文件 | 修改内容 |
|---|---|
| `inc/hw_config.h` | 在 A1 占位基础上，补入 5 个函数声明和 `FunctionalState` 类型（来自 `stm32f10x.h`） |
| `src/main.c` | 空壳 `main` 改为调用 A2 的初始化序列 + `while(1)` |

### 4.2 新建文件

| 文件 | 说明 |
|---|---|
| `src/hw_config.c` | 5 个函数的实现，裁剪自参考例程，仅保留 STM32F103C8 所需 |

### 4.3 工程分组

`project.uvprojx` 的 `src` Group 加入 `hw_config.c`。`hw_config.h` 已在 A1 加入。

---

## 5. 接口设计

### 5.1 `inc/hw_config.h`

```c
#ifndef __HW_CONFIG_H
#define __HW_CONFIG_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x.h"
/*
 * ST USB 库的 usb_lib.h 第一个 #include 就是 hw_config.h，
 * 而 usb_regs.h 直接使用 __IO / uint16_t / uint8_t 等 CMSIS 类型
 * 却不自行包含 stm32f10x.h。因此 hw_config.h 必须作为 USB 库的
 * 类型依赖入口，率先引入 stm32f10x.h，否则 usb_regs.h 解析失败。
 * (TR1-A1 已建立，A2 保留此 include)
 */

/* Exported functions ------------------------------------------------------- */
void Set_System(void);
void Set_USBClock(void);
void USB_Interrupts_Config(void);
void USB_Cable_Config(FunctionalState NewState);
void Enter_LowPowerMode(void);
void Leave_LowPowerMode(void);

#endif /* __HW_CONFIG_H */
```

**变更说明**：在 A1 占位的基础上，删除"TR1-A2 将在此添加..."的注释，补入 6 个函数声明。`FunctionalState` 类型定义在 `stm32f10x.h` 中，无需额外 include。

### 5.2 `src/hw_config.c`

```c
#include "hw_config.h"

/* Private variables ---------------------------------------------------------*/
ErrorStatus HSEStartUpStatus;

/*******************************************************************************
* Function Name  : Set_System
* Description    : Configures GPIO clock and USB DM/DP pins (PA11/PA12)
* Input          : None
* Return         : None
* Note           : 系统时钟 72MHz 已由启动文件 SystemInit() 配置，此处不重做。
*                  本板无 PMOS 上拉开关，PA12 初始设为 Out_PP 输出低，
*                  让主机认为设备未连接，待 PowerOn() 切为 AF_PP。
*******************************************************************************/
void Set_System(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;

  /* 使能 GPIOA 时钟 */
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

  /* PA11 (USB DM) 和 PA12 (USB DP) 配置为复用推挽 */
  /* 但 PA12 先设为普通推挽输出低，模拟设备未连接 */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  GPIO_ResetBits(GPIOA, GPIO_Pin_12);  /* 输出低，拉低 D+ */
}

/*******************************************************************************
* Function Name  : Set_USBClock
* Description    : Configures USB Clock input (48MHz from PLLCLK / 1.5)
* Input          : None
* Return         : None
*******************************************************************************/
void Set_USBClock(void)
{
  /* 选择 USB 时钟源: PLLCLK / 1.5 = 72 / 1.5 = 48MHz */
  RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_1Div5);

  /* 使能 USB 外设时钟 */
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USB, ENABLE);
}

/*******************************************************************************
* Function Name  : USB_Interrupts_Config
* Description    : Configures the USB interrupts (NVIC Group 2, Preemption 0)
* Input          : None
* Return         : None
* Note           : USBWakeUp 中断明确禁用 (TR1 禁用挂起模式, §6.2)
*******************************************************************************/
void USB_Interrupts_Config(void)
{
  NVIC_InitTypeDef NVIC_InitStructure;

  /* 优先级组 2: 2 位抢占优先级, 2 位响应优先级 */
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

  /* USB 低优先级中断 (USB_LP_CAN1_RX0): 抢占 0, 响应 0 */
  NVIC_InitStructure.NVIC_IRQChannel = USB_LP_CAN1_RX0_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);

  /* USB 唤醒中断: 明确禁用 (TR1 禁用挂起模式) */
  NVIC_InitStructure.NVIC_IRQChannel = USBWakeUp_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelCmd = DISABLE;
  NVIC_Init(&NVIC_InitStructure);
}

/*******************************************************************************
* Function Name  : USB_Cable_Config
* Description    : Software Connection/Disconnection of USB Cable
* Input          : NewState - ENABLE (connect) / DISABLE (disconnect)
* Return         : None
* Note           : 本板无 PMOS 开关, 通过 PA12 模式切换实现软件重连。
*                  ENABLE:  PA12 切为 AF_PP, 交由 USB 外设驱动 D+
*                  DISABLE: PA12 切为 Out_PP 输出低, 拉低 D+ 模拟断开
*******************************************************************************/
void USB_Cable_Config(FunctionalState NewState)
{
  GPIO_InitTypeDef GPIO_InitStructure;

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

  if (NewState != DISABLE)
  {
    /* 连接: PA12 切为 AF_PP, 交由 USB 外设 */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
  }
  else
  {
    /* 断开: PA12 切为 Out_PP 输出低, 拉低 D+ */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_ResetBits(GPIOA, GPIO_Pin_12);
  }
}

/*******************************************************************************
* Function Name  : Enter_LowPowerMode
* Description    : Enters low power mode (suspend)
* Input          : None
* Return         : None
* Note           : TR1 禁用挂起模式 (fSuspendEnabled = FALSE), 此函数不会被
*                  调用。提供空实现以满足 usb_pwr.c 的链接依赖。
*******************************************************************************/
void Enter_LowPowerMode(void)
{
  /* TR1 禁用挂起, 空实现 */
}

/*******************************************************************************
* Function Name  : Leave_LowPowerMode
* Description    : Leaves low power mode (resume)
* Input          : None
* Return         : None
* Note           : 同 Enter_LowPowerMode, 空实现。
*******************************************************************************/
void Leave_LowPowerMode(void)
{
  /* TR1 禁用挂起, 空实现 */
}
```

**与参考例程的裁剪对比**：

| 参考例程内容 | 本项目处理 | 原因 |
|---|---|---|
| 多芯片 `#ifdef`（STM32L1XX/F37X/F303x） | 删除，仅保留 STM32F10X_MD 路径 | 本项目只用 STM32F103C8 |
| EVAL 板 LED 函数（`Led_Config` 等 5 个） | 删除 | 本板无 LED |
| `MAL_Config()` / `MAL_Init()` | 删除 | TR1 不涉及存储介质（§5.2） |
| `USB_Disconnect_Config()` | 删除 | 本板无独立 USB_DISCONNECT 引脚 |
| EXTI 唤醒配置 | 删除 | TR1 禁用挂起，不需 EXTI_Line18 |
| `Get_SerialNum()` / `IntToUnicode()` | 删除 | 属 TR1-C2 字符串描述符，非 A2 |
| SDIO 中断配置 | 删除 | 本项目无 SDIO |
| `Set_System()` 中 `MAL_Config()` 调用 | 删除 | TR1 不涉及 |
| USB_LP 抢占优先级 2 | 改为 0 | 文档 §6.5 要求 |
| USBWakeUp 中断使能 | 改为 DISABLE | 文档 §6.2 禁用挂起 |

### 5.3 `src/main.c`

```c
#include "stm32f10x.h"
#include "hw_config.h"

int main(void)
{
  Set_System();
  Set_USBClock();
  USB_Interrupts_Config();
  USB_Cable_Config(ENABLE);   /* PA12 切为 AF_PP, 满足 A2 验收 */

  while (1)
  {
  }
}
```

**变更说明**：A1 的空壳 `main` 改为 A2 的初始化序列。`USB_Cable_Config(ENABLE)` 是 A2 特有的临时调用——B3 的 `PowerOn()` 会接管 PA12 切换职责，届时 `main.c` 改为 `PowerOn()`。这一行的唯一目的是满足 A2 验收标准"PA12 已切为 AF_PP"。

---

## 6. 实现要点与风险

### 6.1 `Set_System()` 中 PA12 的初始状态

**问题**：参考例程把 PA11/PA12 一起设为 AF_PP。本项目无 PMOS 开关，若 PA12 直接 AF_PP，D+ 上拉立即生效，主机可能在初始化完成前发起枚举，导致时序混乱。

**处理**：`Set_System()` 中 PA11 设为 AF_PP（DM 固定复用），PA12 设为 Out_PP 输出低（拉低 D+，模拟设备未连接）。待 `PowerOn()`（B3）或 A2 的 `USB_Cable_Config(ENABLE)` 才把 PA12 切为 AF_PP。

### 6.2 USB 时钟精度

**约束**：USB 全速要求 48MHz ± 0.25%。HSE 8MHz × 9 = 72MHz，72 ÷ 1.5 = 48MHz，正好。若 HSE 不是 8MHz（如 12MHz），PLL 倍频系数需调整，且 USB 分频可能无法精确到 48MHz。

**验证**：`Set_USBClock()` 调用后，可用示波器测 PA12 上的 D+ 信号，或在调试器中查看 `RCC->CFGR` 的 USBPRE 位（bit 22 应为 0，表示 ÷1.5）。

### 6.3 中断优先级

**约束**：文档 §6.5 要求抢占优先级 0。这意味着 USB 中断具有最高优先级，任何其他中断（如 SysTick）都不能打断它。TR1 阶段无其他中断，这一设置是安全的。TR2 引入 SPI 中断时需重新评估优先级分配。

### 6.4 禁用挂起的完整链路

A2 只完成 NVIC 层（USBWakeUp DISABLE）。完整的禁用挂起链路是：

| 层 | 组件 | 状态 |
|---|---|---|
| NVIC | USBWakeUp_IRQn | A2 禁用 ✅ |
| CNTR 掩码 | `IMR_MSK` 含 SUSPM | A1 已配置 ✅ |
| 软件处理 | `fSuspendEnabled = FALSE` | B3 `usb_pwr.c` 待实现 |
| 状态机 | `bDeviceState` 不进 SUSPENDED | B3 待实现 |

A2 禁用 USBWakeUp NVIC 后，即使 CNTR 的 SUSP 位被置位（A1 的 `IMR_MSK` 含 SUSPM），MCU 也不会进入 STOP 模式——因为 `Enter_LowPowerMode()` 是空实现。这一链路在 B3 完整闭环。

---

## 7. 验证流程

### 7.1 编译验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | Keil Build (F7) | 0 Error(s), 0 Warning(s) | 待验证 |
| 2 | 查看 Output | `Program Size` 显示 Code 增大（新增 hw_config.c） | 待验证 |

### 7.2 断点验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | Debug → Start (Ctrl+F5) | 进入调试模式 | 待验证 |
| 2 | 断点设在 `Set_System()` 入口 | F5 命中断点 | 待验证 |
| 3 | 单步执行 4 个初始化函数 | 无 HardFault | 待验证 |
| 4 | F5 全速运行 | 停在 `while(1)` | 待验证 |

### 7.3 PA12 模式验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 全速运行至 `while(1)` | 程序停在死循环 | 待验证 |
| 2 | 调试器查看 `GPIOA->CRH` | bit[19:16]（PA12 配置位）= 0b1011（AF_PP, 50MHz） | 待验证 |
| 3 | 万用表测 PA12 对地电压 | 约 3.3V（D+ 上拉生效）或示波器观察到 USB 信号 | 待验证 |

> **PA12 模式判读**：CRH 寄存器每 4 位控制一个引脚，PA12 对应 bit[19:16]。`GPIO_Mode_AF_PP` + `GPIO_Speed_50MHz` 的编码是 `CNF=10, MODE=11`，即 0b1011。若读到 0b0111（Out_PP），说明 `USB_Cable_Config(ENABLE)` 未执行。

### 7.4 时钟验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 调试器查看 `RCC->CFGR` | bit 22 (USBPRE) = 0（PLLCLK ÷ 1.5） | 待验证 |
| 2 | 调试器查看 `RCC->APB1ENR` | bit 23 (USBEN) = 1（USB 时钟使能） | 待验证 |

---

## 8. 常见问题排查

### 8.1 编译报错：identifier "FunctionalState" is undefined

**原因**：`hw_config.h` 未 `#include "stm32f10x.h"`，或 include 被注释掉。

**修复**：确认 `hw_config.h` 顶部的 `#include "stm32f10x.h"` 存在（A1 已建立，A2 保留）。`FunctionalState` 定义在 `stm32f10x.h` 中。

### 8.2 编译报错：undefined symbol GPIO_Init

**原因**：`stm32f10x_conf.h` 中 `#include "stm32f10x_gpio.h"` 被注释，或 `USE_STDPERIPH_DRIVER` 未定义。

**修复**：A1 已配置，若报错检查 `stm32f10x_conf.h` 和 uvprojx 的 Define。

### 8.3 断点不命中 `Set_System()`

**原因**：`main.c` 未调用 `Set_System()`，或 `main.c` 仍是 A1 的空壳。

**修复**：确认 `main.c` 已按 §5.3 更新。

### 8.4 PA12 读数为 Out_PP 而非 AF_PP

**原因**：`USB_Cable_Config(ENABLE)` 未调用，或调用顺序错误。

**修复**：确认 `main.c` 在 `while(1)` 前调用了 `USB_Cable_Config(ENABLE)`。

### 8.5 插入 USB 后 PC 无反应

**原因**：这是 A2 的**预期行为**。A2 只完成硬件初始化，USB 协议栈尚未工作（无描述符、无标准请求响应），PC 可能短暂出现"未知设备"后消失。

**处理**：A2 不验证 PC 识别，那是 B2 的验收。A2 只验证 PA12 模式和断点可达。

---

## 9. 与下一步的衔接

TR1-A2 完成后，USB 外设物理可用。下一步：

- **TR1-A3**：补全 `inc/usb_conf.h` 的 `EP_NUM`/PMA 地址/端点回调宏（在 A1 的 `IMR_MSK` 基础上扩展）。
- **TR1-A4**：新建 `src/usb_istr.c`，定义 `wIstr`/`pEpInt_IN`/`pEpInt_OUT`（覆盖 `usb_globals.c` 的 weak 占位），实现 `USB_LP_CAN1_RX0_IRQHandler` 和 `USB_Istr()`。

> **A4 依赖 A2**：`USB_LP_CAN1_RX0_IRQHandler` 能命中，前提是 A2 的 `USB_Interrupts_Config()` 已使能 NVIC。若 A2 未做，A4 的中断入口永远不会触发。因此 A2 的 NVIC 验证（§7.2 步骤 3）必须在推进 A4 前确认。

---

*文档结束。*
