# TR1-A3 详细设计：usb_conf 端点与 PMA 配置

## 修改记录

| 版本 | 日期 | 修改内容 | 修改人 |
|---|---|---|---|
| V1.0 | 2026-07-18 | 初始版本 | Copilot |

---

## 文档信息

| 项目 | 内容 |
|---|---|
| 需求编号 | TR1-A3 |
| 需求描述 | `usb_conf.h` 端点数 / PMA 缓冲区地址 / 回调全指向 `NOP_Process` |
| 验收标准 | 编译通过，PMA 布局固定 |
| 所属阶段 | TR1-A — USB 栈骨架可编译运行 |
| 前置文档 | `项目框架设计.md`、`TR1框架设计.md` |
| 前置代码 | TR1-A1 已在 `inc/usb_conf.h` 补入 `IMR_MSK`（USB 库编译硬依赖） |

---

## 1. 需求分解

TR1-A3 的目标是**固定 USB 端点的 PMA 布局**，让 USB 库知道每个端点的缓冲区在 Packet Memory Area 的哪个位置。这是 USB 外设从"物理可用"（A2）到"能收发数据"（A4/B1）之间的必要配置。

本任务只改一个文件：`inc/usb_conf.h`。在 A1 已补入 `IMR_MSK` 的基础上，补全三类配置：

| 配置类别 | 宏 | 作用 |
|---|---|---|
| 端点数量 | `EP_NUM` | 设备用到的端点总数（含 EP0） |
| PMA 缓冲区地址 | `BTABLE_ADDRESS`、`ENDP0_RXADDR`、`ENDP0_TXADDR`、`ENDP1_TXADDR`、`ENDP2_RXADDR` | 各端点缓冲区在 PMA 中的偏移 |
| 端点回调绑定 | `EP1_IN_Callback` ~ `EP7_OUT_Callback` | 端点传输完成时调用的函数，TR1 全指向 `NOP_Process` |

**本任务不写任何新代码**，只是补全 `usb_conf.h` 的宏定义。但这些宏决定了 A4 中断分发和 B1 描述符响应时端点数据能否正确收发。

---

## 2. 关键技术决策

### 2.1 PMA 布局（§6.3，本项目最重要的约束）

PMA（Packet Memory Area）是 USB 外设专用的 512 字节双口 RAM，地址 `0x40006000 ~ 0x400061FF`。USB 库的寄存器访问函数（`SetENDP0RxAddr` 等）接受的是**16-bit 字偏移**（库内部 ×2 转字节地址）。

**历史教训**（文档 §6.3 明确记载）：v0.2.1 曾因 PMA 地址写成字节偏移，导致与 BTABLE 重叠，枚举彻底失败。

**本项目 PMA 布局**（按文档 §6.3）：

```
字偏移   字节偏移  大小      用途
0x00    0x00     32字=64B  BTABLE（端点 0~7 的 Buffer Description Table）
0x20    0x40     32字=64B  ENDP0_RXADDR（EP0 接收）
0x40    0x80     32字=64B  ENDP0_TXADDR（EP0 发送）
0x60    0xC0     32字=64B  ENDP1_TXADDR（EP1 Bulk IN）
0x80    0x100    32字=64B  ENDP2_RXADDR（EP2 Bulk OUT）
0xA0    0x140    96字=192B 空闲（到 PMA 末尾 0xFF / 0x1FF）
```

**与参考例程的关键差异**：

| 宏 | 参考例程值 | 本项目值 | 差异原因 |
|---|---|---|---|
| `BTABLE_ADDRESS` | 0x00 | 0x00 | 一致 |
| `ENDP0_RXADDR` | 0x18 | **0x20** | 参考例程用字节偏移 0x18（=字偏移 0x0C），与 BTABLE 区域重叠风险 |
| `ENDP0_TXADDR` | 0x58 | **0x40** | 同上，参考例程 0x58 是字节偏移 |
| `ENDP1_TXADDR` | 0x98 | **0x60** | 同上 |
| `ENDP2_RXADDR` | 0xD8 | **0x80** | 同上 |

> **注意**：参考例程的注释写的是 "rx/tx buffer base address"，但其值（0x18/0x58/0x98/0xD8）是按字节偏移计算的。本项目严格按文档 §6.3 使用 16-bit 字偏移（0x20/0x40/0x60/0x80），每个缓冲区 32 字 = 64 字节，刚好对齐 64 字节边界，不与 BTABLE 重叠。

**为何本项目更安全**：BTABLE 占 32 字（0x00~0x1F），本项目 ENDP0_RXADDR 从 0x20 开始，刚好紧接 BTABLE 末尾，零重叠。参考例程 ENDP0_RXADDR=0x18 落在 BTABLE 区域内（0x00~0x1F 的 0x18 处），虽然实际只用到 EP0~EP2 的 BTABLE 条目（前 12 字），但布局上存在风险。

### 2.2 端点数量（§6.6）

`EP_NUM = 3`：EP0（控制）+ EP1（Bulk IN）+ EP2（Bulk OUT）。这是 MSC Bulk-Only Transport 的最小端点配置。

### 2.3 端点回调（§6.6）

TR1 阶段不实现任何存储逻辑，EP1/EP2 的传输完成回调全部指向 `NOP_Process`（USB 库自带的空函数，定义在 `usb_core.c:1071`）。TR2 会将 `EP1_IN_Callback` 和 `EP2_OUT_Callback` 替换为真实 BOT 回调。

**与参考例程的差异**：参考例程把 `EP1_IN_Callback` 和 `EP2_OUT_Callback` 注释掉了（`//#define`），因为参考例程在 `usb_bot.c` 中实现了真实回调。本项目 TR1 阶段**必须定义**这两个宏为 `NOP_Process`，否则 `usb_istr.c`（A4）初始化 `pEpInt_IN`/`pEpInt_OUT` 数组时会引用未定义的符号。

### 2.4 IMR_MSK（已在 A1 补入）

`IMR_MSK` 已在 TR1-A1 补入，本任务不修改。A3 完成后 `usb_conf.h` 的三类配置全部就位。

---

## 3. 涉及组件

```
┌─────────────────────────────────────────────────┐
│  usb_conf.h (endpoint 组件的配置头文件)           │
│  ┌───────────────────────────────────────────┐  │
│  │ EP_NUM              端点数量 = 3          │  │
│  │ BTABLE_ADDRESS      BTABLE 偏移 = 0x00    │  │
│  │ ENDP0_RXADDR        EP0 接收 = 0x20       │  │
│  │ ENDP0_TXADDR        EP0 发送 = 0x40       │  │
│  │ ENDP1_TXADDR        EP1 Bulk IN = 0x60    │  │
│  │ ENDP2_RXADDR        EP2 Bulk OUT = 0x80   │  │
│  │ IMR_MSK             中断掩码 (A1 已补)     │  │
│  │ EPx_IN/OUT_Callback 端点回调 = NOP_Process│  │
│  └───────────────────────────────────────────┘  │
└────────────────────┬────────────────────────────┘
                     │ 被引用
        ┌────────────┼────────────┐
        ▼            ▼            ▼
   usb_type.h   usb_sil.c    usb_istr.c (A4)
   (A1 已验证)  (A1 已验证)   pEpInt_IN/OUT 初始化
```

**依赖关系**：
- `usb_conf.h` 被 `usb_type.h:44` include，进而被整个 USB 库引用（A1 已验证）
- `IMR_MSK` 被 `usb_sil.c:73` 引用（A1 已验证）
- `EPx_IN/OUT_Callback` 宏将被 `usb_istr.c`（A4）引用，用于初始化 `pEpInt_IN[7]`/`pEpInt_OUT[7]` 数组
- `ENDPx_xADDR` 宏将被 `usb_prop.c`（B2）的 `MASS_Reset()` 引用，用于设置各端点缓冲区地址

---

## 4. 文件清单

### 4.1 修改文件

| 文件 | 修改内容 |
|---|---|
| `inc/usb_conf.h` | 在 A1 的 `IMR_MSK` 基础上，补入 `EP_NUM`、5 个 PMA 地址宏、14 个端点回调宏 |

### 4.2 新建文件

无。本任务只改 `usb_conf.h`。

### 4.3 工程分组

无变化。`usb_conf.h` 已在 A1 加入 uvprojx 的 src Group。

---

## 5. 接口设计

### 5.1 `inc/usb_conf.h` 完整内容

```c
/**
  ******************************************************************************
  * @file    usb_conf.h
  * @brief   USB 配置头文件 — TR1-A3 正式版本
  *
  *          端点数 / PMA 缓冲区地址 / 端点回调绑定。
  *          IMR_MSK 已在 TR1-A1 补入（USB 库编译硬依赖）。
  ******************************************************************************
  */

#ifndef __USB_CONF_H
#define __USB_CONF_H

/*-------------------------------------------------------------*/
/* EP_NUM                                                      */
/* 设备用到的端点总数（含 EP0）                                  */
/*-------------------------------------------------------------*/
#define EP_NUM                          (3)

/*-------------------------------------------------------------*/
/* Buffer Description Table 与端点缓冲区地址                     */
/*                                                             */
/* 注意：以下为 16-bit 字偏移（库函数内部 ×2 转字节地址）。        */
/* 历史教训：v0.2.1 曾误用字节偏移导致与 BTABLE 重叠，枚举失败。   */
/* 布局见 TR1框架设计.md §6.3。                                 */
/*-------------------------------------------------------------*/

/* BTABLE 基地址（字偏移） */
#define BTABLE_ADDRESS      (0x00)

/* EP0 接收缓冲区（字偏移 0x20 = 字节 0x40，64B） */
#define ENDP0_RXADDR        (0x20)

/* EP0 发送缓冲区（字偏移 0x40 = 字节 0x80，64B） */
#define ENDP0_TXADDR        (0x40)

/* EP1 Bulk IN 发送缓冲区（字偏移 0x60 = 字节 0xC0，64B） */
#define ENDP1_TXADDR        (0x60)

/* EP2 Bulk OUT 接收缓冲区（字偏移 0x80 = 字节 0x100，64B） */
#define ENDP2_RXADDR        (0x80)

/*-------------------------------------------------------------*/
/* ISTR events  (A1 阶段硬依赖, 提前从 A3 引入)                  */
/*-------------------------------------------------------------*/
/*
 * usb_sil.c:73 (USB_SIL_Init) 直接引用 IMR_MSK 设置 CNTR 中断屏蔽,
 * 这是 USB 库的编译硬依赖, A1 必须提供, 否则 usb_sil.c 编译失败。
 *
 * 掩码项遵循 TR1 框架设计 §6.2 "禁用挂起模式" 的约束:
 *   - 包含 CNTR_SUSPM: 收到 SUSP 中断后由软件处理状态 (不进 STOP)
 *   - 包含 CNTR_WKUPM: 唤醒中断位 (虽然 TR1 禁用 WakeUp NVIC, 但掩码
 *                     保留以匹配库的预期; 真正禁用靠 NVIC 层)
 *
 * 各 CNTR_xxxM 位定义来自 usb_regs.h (经 usb_lib.h 间接包含)。
 */
#define IMR_MSK (CNTR_CTRM  | CNTR_WKUPM | CNTR_SUSPM | CNTR_ERRM  | CNTR_SOFM \
                 | CNTR_ESOFM | CNTR_RESETM )

/*-------------------------------------------------------------*/
/* CTR service routines                                        */
/* 端点传输完成回调，TR1 阶段全部指向 NOP_Process（空函数）。      */
/* TR2 将替换 EP1_IN_Callback 和 EP2_OUT_Callback 为真实 BOT 回调。*/
/*-------------------------------------------------------------*/
#define  EP1_IN_Callback   NOP_Process
#define  EP2_IN_Callback   NOP_Process
#define  EP3_IN_Callback   NOP_Process
#define  EP4_IN_Callback   NOP_Process
#define  EP5_IN_Callback   NOP_Process
#define  EP6_IN_Callback   NOP_Process
#define  EP7_IN_Callback   NOP_Process

#define  EP1_OUT_Callback  NOP_Process
#define  EP2_OUT_Callback  NOP_Process
#define  EP3_OUT_Callback  NOP_Process
#define  EP4_OUT_Callback  NOP_Process
#define  EP5_OUT_Callback  NOP_Process
#define  EP6_OUT_Callback  NOP_Process
#define  EP7_OUT_Callback  NOP_Process

#endif /* __USB_CONF_H */
```

**变更说明**：

1. **补入 `EP_NUM = 3`**：EP0 + EP1 + EP2，MSC Bulk-Only 最小配置。
2. **补入 5 个 PMA 地址宏**：严格按文档 §6.3 的 16-bit 字偏移布局，每个缓冲区 32 字 = 64 字节，紧接 BTABLE 末尾零重叠。
3. **补入 14 个端点回调宏**：7 个 IN + 7 个 OUT，全部 `NOP_Process`。参考例程注释掉了 `EP1_IN_Callback` 和 `EP2_OUT_Callback`，本项目必须定义（否则 A4 的 `pEpInt_IN`/`pEpInt_OUT` 初始化会引用未定义符号）。
4. **保留 A1 的 `IMR_MSK`**：原样不动，注释已说明来源。

---

## 6. 实现要点与风险

### 6.1 PMA 字偏移 vs 字节偏移（最大风险点）

**问题**：参考例程的 PMA 地址（0x18/0x58/0x98/0xD8）是字节偏移，直接抄会导致与 BTABLE 重叠。文档 §6.3 明确记载 v0.2.1 的教训。

**验证方法**：A3 完成后无法直接验证 PMA 布局（需 B2 的 `MASS_Reset()` 实际写入 `SetENDP0RxAddr` 等）。但可以通过**静态检查**确认：
- BTABLE 占 0x00~0x1F（32 字）
- ENDP0_RXADDR = 0x20，紧接 BTABLE 末尾，不重叠
- 相邻缓冲区间隔 0x20（32 字 = 64 字节），不重叠

**风险等级**：低（只要严格按文档 §6.3 的值，不抄参考例程）。

### 6.2 端点回调宏的完整性

**问题**：参考例程注释掉了 `EP1_IN_Callback` 和 `EP2_OUT_Callback`，若照抄会导致 A4 编译失败。

**处理**：本项目 14 个宏全部定义，TR2 再替换 EP1_IN/EP2_OUT。

### 6.3 A3 的可观测性

**问题**：A3 只改头文件宏定义，编译通过后插 USB 不会有任何可观测变化（USB 协议栈仍未工作）。

**处理**：这是正常的。A3 的验收标准就是"编译通过，PMA 布局固定"，不要求 PC 端有反应。真正的变化在 A4（中断）+ B1（描述符）之后。

---

## 7. 验证流程

### 7.1 编译验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | Keil Build (F7) | 0 Error(s), 0 Warning(s) | 待验证 |

### 7.2 PMA 布局静态检查

| 步骤 | 检查项 | 预期值 | 通过条件 |
|---|---|---|---|
| 1 | BTABLE 范围 | 0x00 ~ 0x1F（32 字） | ✅ 静态确认 |
| 2 | ENDP0_RXADDR | 0x20，紧接 BTABLE 末尾 | ✅ 静态确认 |
| 3 | 相邻缓冲区间隔 | 0x20（32 字 = 64B），无重叠 | ✅ 静态确认 |
| 4 | 最大占用 | ENDP2_RXADDR + 0x20 = 0xA0，< PMA 末尾 0x100 | ✅ 静态确认 |

### 7.3 USBTreeView 验证（预期无变化）

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 烧录，插 USB | 与 A2 相同："设备描述符请求失败" | 待验证 |

> A3 只是配置头文件，USB 协议栈仍未工作，USBTreeView 表现应与 A2 完全一致。若有变化（如不再识别 Full-Speed），说明改动引入了编译期未暴露的问题。

---

## 8. 常见问题排查

### 8.1 编译报错：identifier "NOP_Process" is undefined

**原因**：`usb_conf.h` 被 `usb_type.h` include，而 `usb_type.h` 在 `usb_lib.h` 的 include 链中位于 `usb_core.h` 之前。`NOP_Process` 声明在 `usb_core.h:259`，若 include 顺序导致 `usb_conf.h` 先被解析，则 `NOP_Process` 未声明。

**分析**：实际上不会发生。`usb_conf.h` 中的 `EP1_IN_Callback` 是**宏定义**，不是变量引用，预处理阶段只做文本替换，不检查符号是否声明。宏在 `usb_istr.c`（A4）中被展开为 `pEpInt_IN[0] = NOP_Process;` 时，`usb_core.h` 已通过 `usb_lib.h` include，`NOP_Process` 已声明。

**结论**：不会报错。若真的报错，检查 `usb_istr.c` 的 include 顺序（应先 `usb_lib.h` 再使用回调宏）。

### 8.2 A4 编译报错：EP1_IN_Callback 未定义

**原因**：照抄参考例程，把 `EP1_IN_Callback` 和 `EP2_OUT_Callback` 注释掉了。

**修复**：确认 14 个回调宏全部定义（§5.1）。

### 8.3 枚举失败，怀疑 PMA 重叠

**原因**：PMA 地址误用字节偏移（抄了参考例程的 0x18/0x58/0x98/0xD8）。

**修复**：确认使用 16-bit 字偏移（0x20/0x40/0x60/0x80）。

---

## 9. 与下一步的衔接

TR1-A3 完成后，USB 端点的 PMA 布局固定。下一步：

- **TR1-A4**：新建 `src/usb_istr.c`，定义 `wIstr`/`pEpInt_IN`/`pEpInt_OUT`（覆盖 `usb_globals.c` 的 weak 占位），实现 `USB_LP_CAN1_RX0_IRQHandler` 和 `USB_Istr()`。A4 会直接使用 A3 定义的 14 个 `EPx_IN/OUT_Callback` 宏初始化回调表。
- **TR1-B2**：`MASS_Reset()` 会使用 A3 定义的 `BTABLE_ADDRESS`/`ENDP0_RXADDR` 等宏设置端点缓冲区。

> **A4 依赖 A3**：`pEpInt_IN[7]` 数组初始化为 `{EP1_IN_Callback, EP2_IN_Callback, ..., EP7_IN_Callback}`，这些宏必须已在 `usb_conf.h` 定义。若 A3 未完成，A4 编译失败。

---

*文档结束。*
