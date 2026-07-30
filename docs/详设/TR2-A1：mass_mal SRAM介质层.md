# TR2-A1 详细设计：mass_mal SRAM 介质层

## 修改记录

| 版本 | 日期 | 修改内容 | 修改人 |
|---|---|---|---|
| V1.0 | 2026-07-30 | 初始版本 | Copilot |

---

## 文档信息

| 项目 | 内容 |
|---|---|
| 需求编号 | TR2-A1 |
| 需求描述 | `mass_mal.c/h` SRAM 介质层（8KB 数组 + Init/Read/Write/GetStatus） |
| 验收标准 | 编译通过 |
| 所属阶段 | TR2-A — BOT 协议栈骨架可编译运行 |
| 前置文档 | `项目框架设计.md`、`TR2框架设计.md` |
| 前置代码 | TR1 全部完成 |
| 完成态参照 | `tmp\Mass_Storage\src\mass_mal.c`（SD 卡版，本项目替换为 SRAM） |

---

## 1. 需求分解

TR2-A1 是 TR2 阶段的第一步，目标是建立介质访问层（MAL），用 SRAM 数组模拟 U 盘存储。MAL 是 BOT/SCSI/buffer 四层架构的最底层，对上屏蔽存储硬件差异，提供统一的 Init/Read/Write/GetStatus 接口。

**与参考例程的关键差异**：参考例程的 MAL 用 SD 卡/NAND Flash，需要复杂的硬件初始化和异步等待。本项目的 MAL 用 SRAM 数组，核心就是 `memcpy`——无需硬件初始化、无需等待、读写对称、断电丢失。

本任务新建两个文件：

| 操作 | 文件 | 内容 |
|---|---|---|
| 新建 | `inc/mass_mal.h` | 常量定义 + 4 个函数声明 + 3 个全局变量 extern |
| 新建 | `src/mass_mal.c` | SRAM 数组 + 4 个函数实现 |

---

## 2. 关键技术决策

### 2.1 SRAM 磁盘容量（§6.1）

STM32F103C8 有 20KB SRAM，分配 **8KB** 给磁盘存储：

```
SRAM 布局:
  0x20000000 ~ 0x20001FFF  8KB  磁盘存储 (sram_disk[8192])
  0x20002000 ~ 0x20004FFF  12KB 程序栈/堆/全局变量
```

| 参数 | 值 |
|---|---|
| 磁盘大小 | 8192 字节 (8KB) |
| 块大小 | 512 字节 (USB MSD 标准) |
| 块数 | 16 |
| 最后 LBA | 15 (0-based) |
| 格式化 | FAT12 (最小 FAT 文件系统，16 扇区可格式化) |

### 2.2 MAL 接口签名（与参考例程一致）

```c
uint16_t MAL_Init(uint8_t lun);
uint16_t MAL_GetStatus(uint8_t lun);
uint16_t MAL_Read(uint8_t lun, uint32_t Memory_Offset, uint32_t *Readbuff, uint16_t Transfer_Length);
uint16_t MAL_Write(uint8_t lun, uint32_t Memory_Offset, uint32_t *Writebuff, uint16_t Transfer_Length);
```

**`uint32_t *Readbuff` 的类型注意**：接口用 `uint32_t *` 是参考例程的历史遗留，实际数据是字节流。SRAM 实现中做 `(uint8_t *)` 强制转换后 `memcpy`。

### 2.3 单 LUN 设计

本项目只有 1 个 LUN（LUN 0）：

| 常量 | 值 | 说明 |
|---|---|---|
| `MAL_OK` | 0 | 操作成功 |
| `MAL_FAIL` | 1 | 操作失败 |
| `MAX_LUN` | 0 | 最大 LUN 号（0 表示仅 LUN 0） |

所有函数只处理 `lun == 0` 的情况，其他值返回 `MAL_FAIL`。

### 2.4 全局变量

参考例程用数组 `Mass_Memory_Size[2]` 支持双 LUN。本项目单 LUN，但仍用数组保持接口兼容（SCSI 层通过 `Mass_Block_Count[0]` 等访问）：

```c
uint32_t Mass_Memory_Size[2];   /* [0] = 8192, [1] 未用 */
uint32_t Mass_Block_Size[2];    /* [0] = 512,  [1] 未用 */
uint32_t Mass_Block_Count[2];   /* [0] = 16,   [1] 未用 */
```

### 2.5 SRAM 的读写特性

| 特性 | SRAM | Flash (TR3) |
|---|---|---|
| 读写对称 | ✅ 读写都是 memcpy | ❌ 写前需擦除 |
| 断电保持 | ❌ 丢失 | ✅ 保持 |
| 读写速度 | 极快（纳秒级） | 慢（页编程毫秒级） |
| 磨损平衡 | 不需要 | 需要（10 万次擦写寿命） |

SRAM 的读写对称性使 MAL 实现极其简单——`MAL_Read` 和 `MAL_Write` 都是 `memcpy`，无需擦除逻辑。

---

## 3. 涉及组件

```
┌─────────────────────────────────────────────────┐
│  mal (存储中间层)                                │
│  ┌───────────────────────────────────────────┐  │
│  │ mass_mal.c / mass_mal.h                   │  │
│  │  ├─ sram_disk[8192]           SRAM 磁盘   │  │
│  │  ├─ Mass_Memory_Size[2]       介质容量    │  │
│  │  ├─ Mass_Block_Size[2]        块大小      │  │
│  │  ├─ Mass_Block_Count[2]       块数        │  │
│  │  ├─ MAL_Init()               初始化(空)  │  │
│  │  ├─ MAL_GetStatus()           填充容量信息│  │
│  │  ├─ MAL_Read()                memcpy 读  │  │
│  │  └─ MAL_Write()               memcpy 写  │  │
│  └───────────────────────────────────────────┘  │
╠══════════════════════════════════════════════════╣
║  被引用 (TR2 后续阶段)                           ║
║  ┌──────────────────────────────────────────┐   ║
║  │ memory.c (TR2-A4/C1/C2)                 │   ║
║  │  Read_Memory → MAL_Read                  │   ║
║  │  Write_Memory → MAL_Write                │   ║
║  └──────────────────────────────────────────┘   ║
║  ┌──────────────────────────────────────────┐   ║
║  │ usb_scsi.c (TR2-B1)                     │   ║
║  │  SCSI_ReadCapacity10 → MAL_GetStatus     │   ║
║  │  SCSI_TestUnitReady → MAL_GetStatus      │   ║
║  └──────────────────────────────────────────┘   ║
╚══════════════════════════════════════════════════╝
```

---

## 4. 文件清单

### 4.1 新建文件

| 文件 | 说明 |
|---|---|
| `src/mass_mal.c` | SRAM 数组 + 4 个函数实现 |
| `inc/mass_mal.h` | 常量 + 函数声明 + 全局变量 extern |

### 4.2 工程分组

`project.uvprojx` 的 `src` Group 加入 `mass_mal.c` 和 `mass_mal.h`。

---

## 5. 接口设计

### 5.1 `inc/mass_mal.h`

```c
#ifndef __MASS_MAL_H
#define __MASS_MAL_H

#include "stm32f10x.h"

#define MAL_OK   0
#define MAL_FAIL 1
#define MAX_LUN  0               /* 仅 LUN 0 */

/* 全局变量 (SCSI 层通过这些变量获取介质容量信息) */
extern uint32_t Mass_Memory_Size[2];
extern uint32_t Mass_Block_Size[2];
extern uint32_t Mass_Block_Count[2];

uint16_t MAL_Init(uint8_t lun);
uint16_t MAL_GetStatus(uint8_t lun);
uint16_t MAL_Read(uint8_t lun, uint32_t Memory_Offset, uint32_t *Readbuff, uint16_t Transfer_Length);
uint16_t MAL_Write(uint8_t lun, uint32_t Memory_Offset, uint32_t *Writebuff, uint16_t Transfer_Length);

#endif /* __MASS_MAL_H */
```

### 5.2 `src/mass_mal.c`

```c
/**
  ******************************************************************************
  * @file    mass_mal.c
  * @brief   SRAM 介质访问层 — TR2-A1
  *
  *          用 8KB SRAM 数组模拟 U 盘存储。
  *          读写对称 (memcpy)，断电丢失。
  *          TR3 将替换为 W25Q32 Flash 实现。
  ******************************************************************************
  */

#include "mass_mal.h"
#include <string.h>

/* ── SRAM 磁盘 (8KB = 16 块 × 512B) ──────────────────────────────────────── */
#define SRAM_DISK_SIZE   8192

static uint8_t sram_disk[SRAM_DISK_SIZE];

/* ── 全局变量 (SCSI 层读取容量信息) ──────────────────────────────────────── */
uint32_t Mass_Memory_Size[2];
uint32_t Mass_Block_Size[2];
uint32_t Mass_Block_Count[2];

/*******************************************************************************
* Function Name  : MAL_Init
* Description    : 初始化介质 (SRAM 无需初始化)
* Input          : lun - 逻辑单元号 (本项目仅 0)
* Return         : MAL_OK / MAL_FAIL
*******************************************************************************/
uint16_t MAL_Init(uint8_t lun)
{
    if (lun != 0) return MAL_FAIL;

    /* SRAM 无需硬件初始化, 清零磁盘内容 */
    memset(sram_disk, 0, SRAM_DISK_SIZE);
    return MAL_OK;
}

/*******************************************************************************
* Function Name  : MAL_GetStatus
* Description    : 查询介质状态, 填充容量信息
* Input          : lun - 逻辑单元号
* Return         : MAL_OK / MAL_FAIL
* Note           : SCSI_ReadCapacity10 / SCSI_TestUnitReady 通过此函数获取容量
*******************************************************************************/
uint16_t MAL_GetStatus(uint8_t lun)
{
    if (lun != 0) return MAL_FAIL;

    Mass_Memory_Size[0]  = SRAM_DISK_SIZE;   /* 8192 字节 */
    Mass_Block_Size[0]   = 512;              /* 512 字节/块 */
    Mass_Block_Count[0]  = SRAM_DISK_SIZE / 512;  /* 16 块 */

    return MAL_OK;
}

/*******************************************************************************
* Function Name  : MAL_Read
* Description    : 从 SRAM 磁盘读取数据
* Input          : lun            - 逻辑单元号
*                  Memory_Offset  - 字节偏移
*                  Readbuff       - 输出缓冲区
*                  Transfer_Length - 读取字节数
* Return         : MAL_OK / MAL_FAIL
*******************************************************************************/
uint16_t MAL_Read(uint8_t lun, uint32_t Memory_Offset, uint32_t *Readbuff, uint16_t Transfer_Length)
{
    if (lun != 0) return MAL_FAIL;
    if (Memory_Offset + Transfer_Length > SRAM_DISK_SIZE) return MAL_FAIL;

    memcpy((uint8_t *)Readbuff, &sram_disk[Memory_Offset], Transfer_Length);
    return MAL_OK;
}

/*******************************************************************************
* Function Name  : MAL_Write
* Description    : 向 SRAM 磁盘写入数据
* Input          : lun            - 逻辑单元号
*                  Memory_Offset  - 字节偏移
*                  Writebuff      - 输入缓冲区
*                  Transfer_Length - 写入字节数
* Return         : MAL_OK / MAL_FAIL
* Note           : SRAM 读写对称, 直接 memcpy 覆盖, 无需擦除
*                  (Flash 需先擦后写, TR3 实现)
*******************************************************************************/
uint16_t MAL_Write(uint8_t lun, uint32_t Memory_Offset, uint32_t *Writebuff, uint16_t Transfer_Length)
{
    if (lun != 0) return MAL_FAIL;
    if (Memory_Offset + Transfer_Length > SRAM_DISK_SIZE) return MAL_FAIL;

    memcpy(&sram_disk[Memory_Offset], (uint8_t *)Writebuff, Transfer_Length);
    return MAL_OK;
}
```

**与参考例程的对比**：

| 参考例程 (SD 卡) | 本项目 (SRAM) | 差异原因 |
|---|---|---|
| `SD_Init()` 硬件初始化 | `memset` 清零 | SRAM 无需硬件初始化 |
| `SD_GetCardInfo()` 查询容量 | 硬编码 8192/512/16 | SRAM 容量固定 |
| `SD_ReadMultiBlocks()` 异步读 | `memcpy` 同步读 | SRAM 无需异步等待 |
| `SD_WriteMultiBlocks()` 异步写 | `memcpy` 同步写 | SRAM 无需异步等待 |
| `SD_WaitWriteOperation()` 等待 | 无 | SRAM 读写即时完成 |
| `MAX_LUN = 1` (双 LUN) | `MAX_LUN = 0` (单 LUN) | 本项目仅 1 个介质 |

---

## 6. 实现要点与风险

### 6.1 A1 的可观测性

A1 的可观测结果只有**编译通过**——MAL 是最底层组件，上层（BOT/SCSI/buffer）尚未创建，无法在 USBTreeView 中观测到任何变化。

这与 TR1-A2/A3 类似——骨架阶段的部分组件只有编译验证。

### 6.2 `Memory_Offset` 的单位

`Memory_Offset` 是**字节偏移**，不是块号（LBA）。SCSI 命令中的 LBA 会被 `memory.c` 转换为字节偏移后传给 MAL：

```
Memory_Offset = LBA × Block_Size = LBA × 512
```

例如 LBA=3 → Memory_Offset=1536，读取从第 1536 字节开始的 512 字节。

### 6.3 边界检查

`MAL_Read`/`MAL_Write` 中有边界检查：
```c
if (Memory_Offset + Transfer_Length > SRAM_DISK_SIZE) return MAL_FAIL;
```

这防止主机请求超出 8KB 范围的数据时 SRAM 越界访问。`memory.c` 层也会做块号检查，但 MAL 层的边界检查是最后一道防线。

### 6.4 `sram_disk` 的初始内容

`MAL_Init` 用 `memset` 清零。格式化前 PC 读取磁盘内容会得到全 0（不是 0xFF——Flash 未擦除时是 0xFF，SRAM 初始化后是 0x00）。

**对格式化的影响**：Windows 格式化时不依赖初始内容，会直接写入 MBR/FAT 表。所以初始 0x00 不影响格式化。

### 6.5 SRAM 占用

`sram_disk[8192]` 是 `static` 全局数组，占用 8KB SRAM。STM32F103C8 有 20KB SRAM，剩余 12KB 给程序栈/堆/其他全局变量。TR1 阶段的代码经编译后约占用几 KB SRAM，12KB 足够。

---

## 7. 验证流程

### 7.1 编译验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | Keil Build (F7) | 0 Error(s), 0 Warning(s) | 待验证 |

### 7.2 静态检查

| 步骤 | 检查项 | 预期值 | 通过条件 |
|---|---|---|---|
| 1 | `sram_disk` 大小 | 8192 字节 | ✅ 静态确认 |
| 2 | `Mass_Block_Count[0]` | 16 | ✅ 静态确认 |
| 3 | `Mass_Block_Size[0]` | 512 | ✅ 静态确认 |
| 4 | `MAX_LUN` | 0 | ✅ 静态确认 |
| 5 | 边界检查 | `Memory_Offset + Transfer_Length > 8192` 时返回 FAIL | ✅ 静态确认 |

### 7.3 USBTreeView 验证（预期无变化）

A1 只新增 MAL 组件，上层（BOT/SCSI）尚未接入。USBTreeView 表现与 TR1-C5 完全一致。

---

## 8. 常见问题排查

### 8.1 编译报错：cannot open source input file "mass_mal.h"

**原因**：`mass_mal.c` 或 `mass_mal.h` 未加入 uvprojx。

**修复**：确认 `project.uvprojx` 的 src Group 已加入 `mass_mal.c` 和 `mass_mal.h`，IncludePath 包含 `.\inc`。

### 8.2 链接报错：undefined symbol Mass_Memory_Size

**原因**：`mass_mal.c` 未加入工程，或 `mass_mal.h` 的 extern 声明与定义不匹配。

**修复**：确认 `mass_mal.c` 已加入 uvprojx，`Mass_Memory_Size` 在 `mass_mal.c` 中定义（非 static）。

### 8.3 SRAM 不足（链接器报错）

**原因**：`sram_disk[8192]` + 其他全局变量 + 栈超过 20KB。

**修复**：减小 `SRAM_DISK_SIZE`（如 4096），或优化其他全局变量。

---

## 9. 与下一步的衔接

TR2-A1 完成后，MAL 层就位。下一步：

- **TR2-A2**：`scsi_data.c/h` SCSI 静态响应数据（Inquiry/SenseData/ModeSense/ReadCapacity）。与 MAL 独立，可并行开发。
- **TR2-A3**：`usb_bot.c/h` BOT 状态机骨架 + `usb_endp.c` 端点回调接管。A3 会引用 A2 的静态数据。
- **TR2-A4**：`memory.c/h` 缓冲调度骨架。A4 会引用 A1 的 MAL 接口。

> **A1 是 TR2 的地基**：MAL 层定义了存储介质的容量和读写方式。后续所有 SCSI 命令的容量报告（READ_CAPACITY）、数据读写（READ10/WRITE10）都依赖 MAL 提供的数据。

---

*文档结束。*
