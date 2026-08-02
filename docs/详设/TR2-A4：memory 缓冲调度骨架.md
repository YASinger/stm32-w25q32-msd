# TR2-A4 详细设计：memory 缓冲调度骨架

## 修改记录

| 版本 | 日期 | 修改内容 | 修改人 |
|---|---|---|---|
| V1.0 | 2026-08-02 | 初始版本 | Copilot |

---

## 文档信息

| 项目 | 内容 |
|---|---|
| 需求编号 | TR2-A4 |
| 需求描述 | `memory.c/h` 缓冲调度骨架（Read_Memory/Write_Memory 框架，暂不调用 MAL） |
| 验收标准 | 编译通过（0 Error 0 Warning），文件就位，无运行行为变化 |
| 所属阶段 | TR2-A — BOT 协议栈骨架可编译运行 |
| 前置文档 | `项目框架设计.md`、`TR2框架设计.md`（§6.5 缓冲调度机制） |
| 前置代码 | TR2-A1 MAL 已完成；TR2-A2 scsi_data 已完成；TR2-A3 BOT 骨架已完成 |
| 完成态参照 | `tmp\Mass_Storage\src\memory.c`、`tmp\Mass_Storage\inc\memory.h` |

---

## 1. 需求分解

TR2-A4 是 TR2-A 骨架阶段的收尾步骤，目标是把 BOT 四层架构中的"缓冲调度层"（buffer）以骨架形式就位：`Read_Memory`/`Write_Memory` 负责 64B 端点包与 512B 逻辑块之间的拆包/组包，但 A4 阶段**不真正调用 MAL**（MAL 调用用 `#if 0` 包裹，C1/C2 与 SCSI READ10/WRITE10 一起恢复）。

**A4 的两件事**：
1. 新建 `memory.h`：`TXFR_IDLE`/`TXFR_ONGOING` 传输状态 + `Read_Memory`/`Write_Memory` 函数声明
2. 新建 `memory.c`：缓冲调度骨架——全局变量（`Data_Buffer[512B]`、`TransferState`、`Block_Read_count` 等）+ 完整函数框架，MAL 调用用 `#if 0` 包裹

**A4 与 A1/A2 同性质**：都是"纯编译期任务"——文件新增、能编译，但没有任何运行行为变化。因为调用 `Read_Memory`/`Write_Memory` 的 SCSI 命令层（`usb_scsi.c`）在 B1 才创建，A4 的 memory 是"孤儿文件"（无调用方）。

本任务新建 2 个文件，修改 1 个文件：

| 操作 | 文件 | 内容 |
|---|---|---|
| 新建 | `inc/memory.h` | TXFR 状态 + Read_Memory/Write_Memory 声明 |
| 新建 | `src/memory.c` | 缓冲调度骨架（MAL 调用 #if 0 包裹） |
| 修改 | `project.uvprojx` | src 组加入 memory.c / memory.h |

---

## 2. 关键技术决策

### 2.1 缓冲调度机制（框架设计 §6.5 落地）

USB 端点包 64 字节，逻辑块 512 字节，`memory.c` 负责拆包/组包：

**读取（READ10，C1 恢复完整功能）**：
1. `MAL_Read()` 从 SRAM 读 512 字节到 `Data_Buffer[512B]`
2. 分 8 次（512/64=8）通过 `USB_SIL_Write(EP1_IN, &Data_Buffer[offset], 64)` 发送
3. 每次 EP1 IN 中断回调 `Read_Memory()` 发送下一包，发完进 `BOT_DATA_IN_LAST`

**写入（WRITE10，C2 恢复完整功能）**：
1. 每次 EP2 OUT 中断回调 `Write_Memory()` 从 `Bulk_Data_Buff[64]` 拷贝到 `Data_Buffer[512B]`
2. 积累满 512 字节后 `MAL_Write()` 写入 SRAM
3. 全部写完后发 CSW

**`Data_Buffer` 的字节数**：完成态定义为 `uint32_t Data_Buffer[BULK_MAX_PACKET_SIZE * 2]` = `uint32_t[128]` = **512 字节**。用 `uint32_t` 而非 `uint8_t` 是 ST 例程写法（保证 4 字节对齐），字节访问时强转 `(uint8_t *)`。

### 2.2 A4 骨架裁剪策略

完成态 `memory.c` 的 `Read_Memory`/`Write_Memory` 引用三个 A4 阶段不可用的东西，处理如下：

| 依赖项 | 来源 | A4 处理 |
|---|---|---|
| `MAL_Read` / `MAL_Write` | `mass_mal.h`（A1 已有，但按框架设计 A4 暂不调用） | `#if 0` 包裹 |
| `Led_RW_ON` / `Led_RW_OFF` | 参考例程 `hw_config.h`，本项目无此函数 | 删除（注释说明） |
| `#include "usb_scsi.h"` | `usb_scsi.h`（B1 才创建） | 注释掉，B1 恢复 |

**为什么 A4 不调用 MAL？** 框架设计 TR2-A4 明确"暂不调用 MAL"，把"真正存取介质"留给 C1/C2（与 SCSI READ10/WRITE10 一起实现）。这样每个阶段的 diff 独立可 review：
- **A4 diff**：新增孤儿文件（无调用方），MAL 调用 `#if 0`
- **C1 diff**：取消 `Read_Memory` 的 `#if 0` + 实现 `usb_scsi.c` READ10
- **C2 diff**：取消 `Write_Memory` 的 `#if 0` + 实现 WRITE10

### 2.3 A4 的 `#if 0` 裁剪点

完成态的 `Read_Memory` 中，`MAL_Read()` 调用是"读介质"的唯一入口；`Write_Memory` 中 `MAL_Write()` 同理。A4 骨架把这两处调用用 `#if 0` 包裹，**其余缓冲调度逻辑（`USB_SIL_Write` 发送、`Data_Buffer` 拷贝、状态机流转）完整保留编译**：

```c
if (!Block_Read_count)
{
#if 0  /* === MAL 读取介质 (C1 恢复) === */
    MAL_Read(lun, Offset, Data_Buffer, Mass_Block_Size[lun]);
#endif
    USB_SIL_Write(EP1_IN, (uint8_t *)Data_Buffer, BULK_MAX_PACKET_SIZE);
    ...
}
```

### 2.4 `Led_RW_ON`/`Led_RW_OFF` 的处理

参考例程在读写时点 LED（`hw_config.c` 实现）。本项目 TR2 框架设计 §5.2 明确 **TR2 不修改 `hw_config.c/h`**，且本项目 `hw_config.h` 无 LED 读写函数。因此 A4 删除这两处调用，以注释标注"C1/C2 若需 LED 读写指示可自行引入"。

### 2.5 `memory.h` 的 include 选择

完成态 `memory.h` 包含 `hw_config.h`（例程习惯，非必要）。本项目 `memory.h` 只需 `uint8_t`/`uint32_t` 类型，改为 `#include "stm32f10x.h"` 实现自包含，避免引入无关依赖。

### 2.6 全局变量 extern 引用

`memory.c` 通过 `extern` 引用 BOT 层与 MAL 层已定义符号：

| 符号 | 定义处 | 用途 |
|---|---|---|
| `Bulk_Data_Buff[64]` | `usb_bot.c` | WRITE 数据接收缓冲 |
| `Data_Len` | `usb_bot.c` | 本次 OUT 实际字节数 |
| `Bot_State` | `usb_bot.c` | 推进 BOT 状态机 |
| `CBW` / `CSW` | `usb_bot.c` | CSW 残差计算 |
| `Mass_Block_Size[2]` | `mass_mal.c` | 块大小（512B）换算 |
| `Mass_Memory_Size[2]` | `mass_mal.c` | 容量信息 |

这些符号 A1/A3 已定义，A4 直接 extern 引用即可。

---

## 3. 涉及组件

```
EP1 IN / EP2 OUT 中断 (usb_istr.c → usb_endp.c → usb_bot.c)
    │
    └── SCSI_Read10_Cmd / SCSI_Write10_Cmd (usb_scsi.c, B1/C1/C2 才接入)
        └── Read_Memory / Write_Memory (memory.c, 本任务)
            ├── Data_Buffer[512B] 拆包/组包
            ├── USB_SIL_Write(EP1_IN) / SetEPRxStatus(ENDP2)
            └── MAL_Read / MAL_Write (mass_mal.c, #if 0 包裹待 C1/C2)
```

A4 阶段 memory 层无调用方（SCSI 层未创建），是纯文件就位。

---

## 4. 文件清单

### 4.1 新建文件

| 文件 | 说明 |
|---|---|
| `inc/memory.h` | TXFR 状态宏 + Read_Memory/Write_Memory 声明 |
| `src/memory.c` | 缓冲调度骨架 |

### 4.2 修改文件

| 文件 | 修改内容 |
|---|---|
| `project.uvprojx` | src 组加入 `memory.c`（FileType 1）和 `memory.h`（FileType 5） |

---

## 5. 接口设计

### 5.1 `inc/memory.h`

```c
#ifndef __MEMORY_H
#define __MEMORY_H

#include "stm32f10x.h"   /* uint8_t/uint32_t 类型 */

/* 传输状态：A4 骨架用 TXFR_ONGOING 区分多包续传的首次进入 */
#define TXFR_IDLE     0
#define TXFR_ONGOING  1

/* Read_Memory: 将介质数据拆包发送到 EP1 IN (READ10 多包调度)
 * Write_Memory: 将 EP2 OUT 数据组包写入介质 (WRITE10 多包调度)
 * lun - 逻辑单元号; Memory_Offset - LBA; Transfer_Length - 块数 */
void Read_Memory(uint8_t lun, uint32_t Memory_Offset, uint32_t Transfer_Length);
void Write_Memory(uint8_t lun, uint32_t Memory_Offset, uint32_t Transfer_Length);

#endif /* __MEMORY_H */
```

### 5.2 `src/memory.c`

骨架版，两处 MAL 调用用 `#if 0` 包裹，删除 `Led_RW_*`：

```c
/* Includes */
#include "memory.h"
#include "usb_bot.h"      /* BULK_MAX_PACKET_SIZE / BOT_* / Set_CSW */
#include "mass_mal.h"     /* Mass_Block_Size / MAL_Read / MAL_Write */
#include "usb_lib.h"      /* USB_SIL_Write / SetEPTxCount / SetEPxStatus */

/* B1 恢复: #include "usb_scsi.h" */

/* 全局变量 */
__IO uint32_t Block_Read_count = 0;
__IO uint32_t Block_offset;
__IO uint32_t Counter = 0;
uint32_t Idx;
uint32_t Data_Buffer[BULK_MAX_PACKET_SIZE * 2];   /* 512 字节 */
uint8_t TransferState = TXFR_IDLE;

/* extern 引用 BOT/MAL 层符号 */
extern uint8_t Bulk_Data_Buff[BULK_MAX_PACKET_SIZE];
extern uint16_t Data_Len;
extern uint8_t Bot_State;
extern Bulk_Only_CBW CBW;
extern Bulk_Only_CSW CSW;
extern uint32_t Mass_Memory_Size[2];
extern uint32_t Mass_Block_Size[2];

/* 读介质 → EP1 IN 拆包发送（READ10 多包调度，C1 恢复完整功能）
 * 首次进入：计算字节偏移/长度，置 TXFR_ONGOING
 * 每包 64B：USB_SIL_Write 发送，EP1 IN 中断回调再次进入，直到 Length==0 */
void Read_Memory(uint8_t lun, uint32_t Memory_Offset, uint32_t Transfer_Length)
{
  static uint32_t Offset, Length;

  if (TransferState == TXFR_IDLE )
  {
    Offset = Memory_Offset * Mass_Block_Size[lun];
    Length = Transfer_Length * Mass_Block_Size[lun];
    TransferState = TXFR_ONGOING;
  }

  if (TransferState == TXFR_ONGOING )
  {
    if (!Block_Read_count)
    {
#if 0  /* === MAL 读介质到 Data_Buffer (C1 恢复) === */
      MAL_Read(lun ,
               Offset ,
               Data_Buffer,
               Mass_Block_Size[lun]);
#endif
      USB_SIL_Write(EP1_IN, (uint8_t *)Data_Buffer, BULK_MAX_PACKET_SIZE);

      Block_Read_count = Mass_Block_Size[lun] - BULK_MAX_PACKET_SIZE;
      Block_offset = BULK_MAX_PACKET_SIZE;
    }
    else
    {
      USB_SIL_Write(EP1_IN, (uint8_t *)Data_Buffer + Block_offset, BULK_MAX_PACKET_SIZE);

      Block_Read_count -= BULK_MAX_PACKET_SIZE;
      Block_offset += BULK_MAX_PACKET_SIZE;
    }

    SetEPTxCount(ENDP1, BULK_MAX_PACKET_SIZE);
    SetEPTxStatus(ENDP1, EP_TX_VALID);
    Offset += BULK_MAX_PACKET_SIZE;
    Length -= BULK_MAX_PACKET_SIZE;

    CSW.dDataResidue -= BULK_MAX_PACKET_SIZE;
    /* Led_RW_ON();  TR2 不引入 LED 读写指示 (见 §2.4) */
  }

  if (Length == 0)
  {
    Block_Read_count = 0;
    Block_offset = 0;
    Offset = 0;
    Bot_State = BOT_DATA_IN_LAST;
    TransferState = TXFR_IDLE;
    /* Led_RW_OFF();  TR2 不引入 LED 读写指示 */
  }
}

/* EP2 OUT 组包 → 写介质（WRITE10 多包调度，C2 恢复完整功能）
 * 每包 64B 从 Bulk_Data_Buff 拷入 Data_Buffer，凑满 512B 触发介质写，
 * 全部写完（W_Length==0）或上层要求发 CSW 时结束 */
void Write_Memory (uint8_t lun, uint32_t Memory_Offset, uint32_t Transfer_Length)
{
  static uint32_t W_Offset, W_Length;

  uint32_t temp = Counter + BULK_MAX_PACKET_SIZE;

  if (TransferState == TXFR_IDLE )
  {
    W_Offset = Memory_Offset * Mass_Block_Size[lun];
    W_Length = Transfer_Length * Mass_Block_Size[lun];
    TransferState = TXFR_ONGOING;
  }

  if (TransferState == TXFR_ONGOING )
  {
    for (Idx = 0 ; Counter < temp; Counter++)
    {
      *((uint8_t *)Data_Buffer + Counter) = Bulk_Data_Buff[Idx++];
    }

    W_Offset += Data_Len;
    W_Length -= Data_Len;

    if (!(W_Length % Mass_Block_Size[lun]))
    {
      Counter = 0;
#if 0  /* === MAL 将 Data_Buffer 写入介质 (C2 恢复) === */
      MAL_Write(lun ,
                W_Offset - Mass_Block_Size[lun],
                Data_Buffer,
                Mass_Block_Size[lun]);
#endif
    }

    CSW.dDataResidue -= Data_Len;
    SetEPRxStatus(ENDP2, EP_RX_VALID); /* enable the next transaction */
    /* Led_RW_ON();  TR2 不引入 LED 读写指示 */
  }

  if ((W_Length == 0) || (Bot_State == BOT_CSW_Send))
  {
    Counter = 0;
    Set_CSW (CSW_CMD_PASSED, SEND_CSW_ENABLE);
    TransferState = TXFR_IDLE;
    /* Led_RW_OFF();  TR2 不引入 LED 读写指示 */
  }
}
```

**与完成态的差异**：

| 差异点 | 完成态 | A4（当前） | 恢复时机 |
|---|---|---|---|
| `#include "usb_scsi.h"` | 有 | 注释掉 | B1 |
| `#include "hw_config.h"` | 有 | 删除（memory.h 自包含 stm32f10x.h） | — |
| `#include "usb_regs.h"` / `usb_mem.h` / `usb_conf.h` | 有 | 用 `usb_lib.h` 合并替代 | — |
| `MAL_Read()` 调用 | 有 | `#if 0` 包裹 | C1 |
| `MAL_Write()` 调用 | 有 | `#if 0` 包裹 | C2 |
| `Led_RW_ON()` / `Led_RW_OFF()` | 有 | 删除（注释说明） | 可选，TR2 不做 |

> **注意**：`temp` 用 `BULK_MAX_PACKET_SIZE` 表达（=64），替代参考例程硬编码的 `64`，保持语义一致且避免魔法数字。

---

## 6. 实现要点与风险

### 6.1 A4 无可观测运行变化

A4 与 A1/A2 同性质：memory 层无调用方（`Read_Memory`/`Write_Memory` 仅在 `usb_scsi.c` 的 READ10/WRITE10 命令中被调用，而 `usb_scsi.c` 是 B1 才创建）。因此：

- **A3 → A4 的 USBTreeView 输出应完全一致**（Problem Code 10，CBW 响应 FAILED，无数据阶段）
- 唯一差异体现在 `.map` 文件：新增 `memory.o` 段，`Read_Memory`/`Write_Memory` 符号存在于链接结果（但无引用方，可能被 `--remove` 优化掉，属正常）
- **若 A4 后 USBTreeView 有变化 → 说明 A4 引入了意外链接**，需排查是否有文件错误引用了 memory.c 符号

### 6.2 `#if 0` 块内代码不参与编译

`MAL_Read`/`MAL_Write` 的 `#if 0` 包裹意味着：

- 编译期：`MAL_Read(lun, Offset, Data_Buffer, Mass_Block_Size[lun])` 不生成任何代码
- `MAL_Read`/`MAL_Write` 的**函数原型**仍来自 `mass_mal.h`（include 保留），因此即使将来取消 `#if 0` 也不需要加 include
- `Mass_Block_Size[lun]` 是 `extern` 数组，`#if 0` 块外仍被使用（`Offset`/`Length` 换算、`Block_Read_count` 初始化），所以 `mass_mal.h` 的 include 是必需的

### 6.3 无调用方的孤儿函数与链接优化

A4 阶段 `Read_Memory`/`Write_Memory` 无任何调用方。Keil 默认不开启 `--remove` 时仍会链接进固件；若开启 `One ELF Section per Function` 则可能被剔除。无论哪种情况都不影响运行——本任务只要求"能编译、文件就位"。

### 6.4 数据类型一致性

| 符号 | 类型 | 说明 |
|---|---|---|
| `Data_Buffer` | `uint32_t[128]` | 512 字节，4 字节对齐；字节访问强转 `(uint8_t *)` |
| `Bulk_Data_Buff` | `uint8_t[64]` | BOT 层 OUT 接收缓冲 |
| `Block_Read_count` | `__IO uint32_t` | 剩余待发字节数（首包后 = 512-64=448） |
| `Block_offset` | `__IO uint32_t` | 当前发送偏移 |

### 6.5 潜在风险：`static` 局部变量与中断重入

`Read_Memory`/`Write_Memory` 的 `Offset`/`Length`/`W_Offset`/`W_Length` 是 `static` 局部变量，状态跨中断回调保留。A4 阶段无调用方，无重入风险；C1/C2 恢复时保持"单传输进行中"的 BOT 状态机约束（一次只有一个 READ10/WRITE10 在途），`TransferState` 保证首次进入才重算偏移。

---

## 7. 验证流程

### 7.1 编译验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | Keil Build (F7) | 0 Error(s), 0 Warning(s) | 待验证 |
| 2 | 查看 Build Output | 出现 `memory.c` 编译行，无 `undefined symbol` | 待验证 |

### 7.2 链接验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 查看 `.map` 文件 | `memory.o` 已加入链接（若 `--remove` 未开） | 待验证 |
| 2 | 查看 `.map` 文件 | `Data_Buffer`、`TransferState`、`Block_Read_count` 等符号存在 | 待验证 |

### 7.3 USBTreeView 验证（回归确认）

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 烧录，插 USB | 与 A3 一致的枚举结果 | 待验证 |
| 2 | Problem Code | 仍为 10（与 A3 相同） | 待验证 |
| 3 | Pipe[0]/Pipe[1] | 仍为 EP1 IN / EP2 OUT | 待验证 |

> A4 是纯编译期任务，USBTreeView 的唯一作用是**确认无意外回归**。

---

## 8. 常见问题排查

### 8.1 编译报错：undefined symbol MAL_Read / MAL_Write

**原因**：`#if 0` 块未生效或 include 缺失。若 `#if 0` 包裹不完整，`MAL_Read` 调用会参与编译，但 `mass_mal.c` 未加入工程导致链接失败。

**修复**：确认 `mass_mal.h` 已 include（原型可见），`MAL_Read`/`MAL_Write` 调用体在 `#if 0` 内。若 `mass_mal.c` 未加入工程，则先补工程文件（A1 已加入）。

### 8.2 编译报错：Led_RW_ON undefined

**原因**：`Led_RW_*` 调用未删除，但本项目 `hw_config.h` 无此函数。

**修复**：按 §5.2 骨架删除 `Led_RW_ON()`/`Led_RW_OFF()` 调用（或注释）。

### 8.3 编译报错：unknown type name 'Bulk_Only_CBW' / 'Bulk_Only_CSW'

**原因**：`usb_bot.h` 未 include 或 include 顺序问题（`memory.c` 用到了 CBW/CSW 类型）。

**修复**：确认 `#include "usb_bot.h"` 在 `memory.c` 的 include 区。

### 8.4 编译报错：Set_CSW / CSW_CMD_PASSED / SEND_CSW_ENABLE undefined

**原因**：`usb_bot.h` 未 include（`Set_CSW` 原型与 `CSW_CMD_PASSED`/`SEND_CSW_ENABLE` 宏都在其中）。

**修复**：确认 `#include "usb_bot.h"`。

### 8.5 编译报错：EP1_IN / ENDP1 / EP_TX_VALID undefined

**原因**：`usb_regs.h` 未进入编译单元（这些宏定义在 `usb_regs.h`，通常经 `usb_lib.h` 间接包含）。

**修复**：确认 `#include "usb_lib.h"`（它包含 `usb_regs.h`）。

### 8.6 编译报错：USB_SIL_Write / SetEPTxCount / SetEPRxStatus undefined

**原因**：`usb_sil.h` / `usb_regs.h` 的函数原型未可见。

**修复**：确认 `#include "usb_lib.h"`（同时包含 `usb_sil.h` 与 `usb_regs.h`）。

### 8.7 链接后 USBTreeView 行为与 A3 不同

**原因**：A4 引入了意外链接（某个已编译文件引用了 memory 符号），或 flash 占用变化导致异常。

**修复**：对照 §6.1，确认 memory 层无调用方；比对 A3 与 A4 的 `.map` 差异，定位多余引用。

---

## 9. 与下一步的衔接

TR2-A4 完成后，缓冲调度层骨架就位，BOT 四层架构中"传输层（BOT）+ 缓冲层（memory）"已备齐。下一步：

- **TR2-B1**：`usb_scsi.c/h` SCSI 查询命令实现（Inquiry/ReadCapacity10/TestUnitReady 等，不含 READ10/WRITE10 数据命令）。B1 取消 `usb_bot.c` 中 `CBW_Decode()` 的 `#if 0` 包裹，加回 `#include "usb_scsi.h"`；同时恢复 `memory.c` 中 `#include "usb_scsi.h"` 注释。Problem Code 10 消失。
- **TR2-C1**：READ10 数据通路。取消 `Read_Memory` 中 `MAL_Read` 的 `#if 0`，实现 `usb_scsi.c` 的 `SCSI_Read10_Cmd`，接入 `Mass_Storage_In()` 的 `BOT_DATA_IN` 分支（取消其 `#if 0`）。
- **TR2-C2**：WRITE10 数据通路。取消 `Write_Memory` 中 `MAL_Write` 的 `#if 0`，实现 `SCSI_Write10_Cmd`，接入 `Mass_Storage_Out()` 的 `BOT_DATA_OUT` 分支（取消其 `#if 0`）。

> **A4 的定位**：TR2-A 骨架阶段"纯编译期任务"的最后一环。它与 A1（MAL）、A2（scsi_data）、A3（BOT）合起来构成 BOT 四层架构（`usb_endp.c → usb_bot.c → usb_scsi.c → memory.c → mass_mal.c`）的完整文件骨架。此后 B/C 阶段只做"取消 `#if 0` + 实现具体命令"，不再新增文件结构。

---

*文档结束。*
