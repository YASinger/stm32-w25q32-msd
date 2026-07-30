# TR2-A2 详细设计：scsi_data SCSI 静态响应数据

## 修改记录

| 版本 | 日期 | 修改内容 | 修改人 |
|---|---|---|---|
| V1.0 | 2026-07-30 | 初始版本 | Copilot |

---

## 文档信息

| 项目 | 内容 |
|---|---|
| 需求编号 | TR2-A2 |
| 需求描述 | `scsi_data.c/h` SCSI 静态响应数据（Inquiry/SenseData/ModeSense/ReadCapacity/ReadFormatCapacity） |
| 验收标准 | 编译通过 |
| 所属阶段 | TR2-A — BOT 协议栈骨架可编译运行 |
| 前置文档 | `项目框架设计.md`、`TR2框架设计.md` |
| 前置代码 | TR2-A1 MAL 介质层已完成 |
| 完成态参照 | `tmp\Mass_Storage\src\scsi_data.c`（参考例程，本项目调整产品名） |

---

## 1. 需求分解

TR2-A2 的目标是定义 SCSI 命令响应所需的静态数据数组。这些数据在主机发 INQUIRY/REQUEST_SENSE/MODE_SENSE 等查询命令时返回，内容固定（不由 MAL 动态生成）。

**与参考例程的关键差异**：
1. 参考例程有 `Standard_Inquiry_Data2`（NAND 版），本项目只有 SRAM，只需 1 个
2. 参考例程的 Inquiry 产品名是 "SD Flash Disk"，本项目改为 "STM32 SRAM Disk"（TR3 换 Flash 时改）
3. 参考例程没有独立 `scsi_data.h`（声明在 `usb_scsi.h` 中），本项目创建独立的 `scsi_data.h`

本任务新建两个文件：

| 操作 | 文件 | 内容 |
|---|---|---|
| 新建 | `inc/scsi_data.h` | 7 个数组的 extern 声明 + 数据长度宏 |
| 新建 | `src/scsi_data.c` | 7 个静态数据数组定义 |

---

## 2. 关键技术决策

### 2.1 SCSI Inquiry Data 格式（36 字节）

SCSI INQUIRY 响应是固定格式 36 字节：

| 偏移 | 长度 | 字段 | 本项目值 | 说明 |
|---|---|---|---|---|
| 0 | 1 | Peripheral Device Type | 0x00 | Direct Access（块设备） |
| 1 | 1 | RMB | 0x80 | Removable Medium（可移动介质） |
| 2 | 1 | Version | 0x02 | No conformance claim |
| 3 | 1 | Response Format | 0x02 | — |
| 4 | 1 | Additional Length | 36-4=32 | 后续数据长度 |
| 5-7 | 3 | Reserved | 0x00 | — |
| 8-15 | 8 | Vendor Identification | "STM     " | 3字符+5空格 |
| 16-31 | 16 | Product Identification | "STM32 SRAM Disk  " | 15字符+1空格 |
| 32-35 | 4 | Product Revision Level | "1.0 " | 3字符+1空格 |

**与参考例程的差异**：参考例程 Vendor="STM     "，Product="SD Flash Disk   "。本项目 Product 改为 "STM32 SRAM Disk  "，TR3 时改为 "STM32 W25Q32 Disk"。

### 2.2 Sense Data 格式（18 字节）

REQUEST_SENSE 命令返回的错误信息：

| 偏移 | 长度 | 字段 | 初始值 | 说明 |
|---|---|---|---|---|
| 0 | 1 | Response Code | 0x70 | Fixed format |
| 1 | 1 | Segment Number | 0x00 | — |
| 2 | 1 | Sense Key | NO_SENSE (0) | 初始无错误 |
| 3-6 | 4 | Information | 0x00 | — |
| 7 | 1 | Additional Sense Length | 0x0A | 10 字节后续 |
| 8-11 | 4 | Reserved | 0x00 | — |
| 12 | 1 | ASC (Additional Sense Code) | NO_SENSE (0) | 初始无错误 |
| 13 | 1 | ASCQ | 0x00 | — |
| 14-17 | 4 | Sense Key Specific | 0x00 | — |

**`NO_SENSE` 的值**：0。`Scsi_Sense_Data` 初始全 NO_SENSE，运行时由 `Set_Scsi_Sense_Data()` 修改。

### 2.3 ReadCapacity10_Data 和 ReadFormatCapacity_Data 的动态填充

这两个数组的容量字段初始为 0——因为容量信息由 `MAL_GetStatus()` 动态填充：

- `ReadCapacity10_Data`：8 字节，前 4 字节是最后 LBA，后 4 字节是块大小
- `ReadFormatCapacity_Data`：12 字节，含容量列表长度 + 块数 + 块大小

SCSI 命令处理函数（TR2-B1）在运行时用 `Mass_Block_Count[0]` 和 `Mass_Block_Size[0]` 填充这些字段。A2 只定义初始全 0 的数组。

### 2.4 ModeSense 数据

| 数组 | 长度 | 内容 | 说明 |
|---|---|---|---|
| `Mode_Sense6_data` | 4 字节 | `{0x03, 0x00, 0x00, 0x00}` | ModeSense6 响应 |
| `Mode_Sense10_data` | 8 字节 | `{0x00, 0x06, 0x00, ...}` | ModeSense10 响应 |

### 2.5 Page00_Inquiry_Data

INQUIRY 命令的 Vital Product Data Page 0x00 响应，5 字节，表示支持哪些 Page。本项目只支持 Page 0x00（标准 Inquiry）。

### 2.6 `scsi_data.h` 的设计

参考例程没有独立的 `scsi_data.h`——静态数据的 extern 声明放在 `usb_scsi.h` 中。本项目将其拆分到 `scsi_data.h`，使数据定义（`scsi_data.c`）和数据声明（`scsi_data.h`）成对出现，与 TR1 的 `usb_desc.c/h` 模式一致。

`usb_scsi.h`（TR2-B1 创建）将 `#include "scsi_data.h"`，获取这些数组的声明。

---

## 3. 涉及组件

```
┌─────────────────────────────────────────────────┐
│  scsi_data (数据层)                              │
│  ┌───────────────────────────────────────────┐  │
│  │ scsi_data.c / scsi_data.h                 │  │
│  │  ├─ Page00_Inquiry_Data[5]    VPD Page 0 │  │
│  │  ├─ Standard_Inquiry_Data[36] 标准Inquiry│  │
│  │  ├─ Mode_Sense6_data[4]      ModeSense6  │  │
│  │  ├─ Mode_Sense10_data[8]     ModeSense10 │  │
│  │  ├─ Scsi_Sense_Data[18]      Sense Data   │  │
│  │  ├─ ReadCapacity10_Data[8]   (运行时填充) │  │
│  │  └─ ReadFormatCapacity_Data[12](运行时填充)│  │
│  └───────────────────────────────────────────┘  │
╠══════════════════════════════════════════════════╣
║  被引用 (TR2-B1)                                 ║
║  ┌──────────────────────────────────────────┐   ║
║  │ usb_scsi.c (TR2-B1)                     │   ║
║  │  SCSI_Inquiry_Cmd → Standard_Inquiry_Data│   ║
║  │  SCSI_RequestSense_Cmd → Scsi_Sense_Data │   ║
║  │  SCSI_ReadCapacity10_Cmd → ReadCapacity10_Data│
║  │  SCSI_ModeSense6_Cmd → Mode_Sense6_data │   ║
║  └──────────────────────────────────────────┘   ║
╚══════════════════════════════════════════════════╝
```

---

## 4. 文件清单

### 4.1 新建文件

| 文件 | 说明 |
|---|---|
| `src/scsi_data.c` | 7 个静态数据数组定义 |
| `inc/scsi_data.h` | 7 个数组 extern 声明 + 数据长度宏 |

### 4.2 工程分组

`project.uvprojx` 的 `src` Group 加入 `scsi_data.c` 和 `scsi_data.h`。

---

## 5. 接口设计

### 5.1 `inc/scsi_data.h`

```c
#ifndef __SCSI_DATA_H
#define __SCSI_DATA_H

#include "stm32f10x.h"

/* 数据长度宏 (SCSI 命令处理函数引用) */
#define READ_FORMAT_CAPACITY_DATA_LEN   0x0C    /* 12 */
#define READ_CAPACITY10_DATA_LEN        0x08    /* 8  */
#define MODE_SENSE10_DATA_LEN           0x08    /* 8  */
#define MODE_SENSE6_DATA_LEN            0x04    /* 4  */
#define REQUEST_SENSE_DATA_LEN          0x12    /* 18 */
#define STANDARD_INQUIRY_DATA_LEN       0x24    /* 36 */

/* 静态数据数组 */
extern uint8_t Page00_Inquiry_Data[];
extern uint8_t Standard_Inquiry_Data[];
extern uint8_t Mode_Sense6_data[];
extern uint8_t Mode_Sense10_data[];
extern uint8_t Scsi_Sense_Data[];
extern uint8_t ReadCapacity10_Data[];
extern uint8_t ReadFormatCapacity_Data[];

#endif /* __SCSI_DATA_H */
```

### 5.2 `src/scsi_data.c`

```c
/**
  ******************************************************************************
  * @file    scsi_data.c
  * @brief   SCSI 静态响应数据 — TR2-A2
  *
  *          主机发 INQUIRY/REQUEST_SENSE/MODE_SENSE 等查询命令时
  *          返回的预填充数据。ReadCapacity 的容量字段运行时填充。
  ******************************************************************************
  */

#include "scsi_data.h"

/* ── Vital Product Data Page 0x00 ────────────────────────────────────────── */
uint8_t Page00_Inquiry_Data[] = {
    0x00,   /* PERIPHERAL QUALIFIER & PERIPHERAL DEVICE TYPE */
    0x00,
    0x00,
    0x00,
    0x00    /* Supported Pages: 00 */
};

/* ── Standard Inquiry Data (36 字节) ─────────────────────────────────────── */
uint8_t Standard_Inquiry_Data[] = {
    0x00,       /* Direct Access Device */
    0x80,       /* RMB = 1: Removable Medium */
    0x02,       /* Version: No conformance claim to standard */
    0x02,
    36 - 4,     /* Additional Length */
    0x00,       /* SCCS */
    0x00,
    0x00,
    /* Vendor Identification (8 字节) */
    'S', 'T', 'M', ' ', ' ', ' ', ' ', ' ',
    /* Product Identification (16 字节) */
    'S', 'T', 'M', '3', '2', ' ', 'S', 'R', 'A', 'M', ' ', 'D', 'i', 's', 'k', ' ',
    /* Product Revision Level (4 字节) */
    '1', '.', '0', ' '
};

/* ── Mode Sense6 Data (4 字节) ──────────────────────────────────────────── */
uint8_t Mode_Sense6_data[] = {
    0x03,
    0x00,
    0x00,
    0x00,
};

/* ── Mode Sense10 Data (8 字节) ─────────────────────────────────────────── */
uint8_t Mode_Sense10_data[] = {
    0x00,
    0x06,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00
};

/* ── Sense Data (18 字节, 运行时由 Set_Scsi_Sense_Data 修改) ─────────────── */
uint8_t Scsi_Sense_Data[] = {
    0x70,   /* Response Code: Fixed format */
    0x00,   /* Segment Number */
    0x00,   /* Sense Key: NO_SENSE (运行时修改) */
    0x00,
    0x00,
    0x00,
    0x00,   /* Information */
    0x0A,   /* Additional Sense Length: 10 */
    0x00,
    0x00,
    0x00,
    0x00,   /* Cmd Information */
    0x00,   /* ASC (运行时修改) */
    0x00,   /* ASCQ */
    0x00,   /* FRUC */
    0x00,   /* TBD */
    0x00,
    0x00    /* Sense Key Specific */
};

/* ── Read Capacity10 Data (8 字节, 运行时填充容量) ──────────────────────── */
uint8_t ReadCapacity10_Data[] = {
    /* Last Logical Block (4 字节, 运行时填 Mass_Block_Count-1) */
    0, 0, 0, 0,
    /* Block Length (4 字节, 运行时填 Mass_Block_Size=512=0x200) */
    0, 0, 0, 0
};

/* ── Read Format Capacity Data (12 字节, 运行时填充容量) ────────────────── */
uint8_t ReadFormatCapacity_Data[] = {
    0x00,
    0x00,
    0x00,
    0x08,   /* Capacity List Length: 8 */
    /* Block Count (4 字节, 运行时填) */
    0, 0, 0, 0,
    /* Block Length (4 字节) */
    0x02,   /* Descriptor Code: Formatted Media */
    0, 0, 0
};
```

**与参考例程的差异**：

| 项 | 参考例程 | 本项目 | 原因 |
|---|---|---|---|
| `Standard_Inquiry_Data2` | 有（NAND 版） | 删除 | 本项目只有 SRAM，无需第二组 |
| Product Identification | "SD Flash Disk   " | "STM32 SRAM Disk " | 介质不同 |
| `scsi_data.h` | 无（声明在 usb_scsi.h） | 有（独立头文件） | 数据定义/声明成对 |
| `NO_SENSE` 等宏 | 在 usb_scsi.h | 不在 scsi_data.h | Sense Key 宏属 TR2-B1 |

---

## 6. 实现要点与风险

### 6.1 A2 的可观测性

A2 的可观测结果只有**编译通过**——静态数据数组只定义不使用，上层（SCSI 命令处理）尚未创建。

### 6.2 `Scsi_Sense_Data` 的非 const 属性

`Scsi_Sense_Data` 是 `uint8_t`（非 const），因为 `Set_Scsi_Sense_Data()` 在运行时修改 Sense Key 和 ASC。其他数组也是非 const——虽然大部分内容不变，但 `ReadCapacity10_Data` 和 `ReadFormatCapacity_Data` 的容量字段运行时填充。

### 6.3 Inquiry 产品名的选择

本项目用 "STM32 SRAM Disk"（15 字符 + 1 空格 = 16 字节）。TR3 换 W25Q32 时改为 "STM32 W25Q32 Disk"（16 字符，刚好 16 字节）。

**与 USB 字符串描述符的关系**：USB 字符串描述符的产品名是 "STM32 W25Q32 Flash Disk"（23 字符），SCSI Inquiry 的产品名是 "STM32 SRAM Disk"（15 字符）。两者独立——USB 描述符是 USB 层的身份信息，SCSI Inquiry 是 SCSI 层的介质信息。Windows 设备管理器显示的是 USB 字符串描述符的产品名，磁盘管理器显示的是 SCSI Inquiry 的产品名。

---

## 7. 验证流程

### 7.1 编译验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | Keil Build (F7) | 0 Error(s), 0 Warning(s) | 待验证 |

### 7.2 静态检查

| 步骤 | 检查项 | 预期值 | 通过条件 |
|---|---|---|---|
| 1 | `Standard_Inquiry_Data` 长度 | 36 字节 | ✅ 静态确认 |
| 2 | `Scsi_Sense_Data` 长度 | 18 字节 | ✅ 静态确认 |
| 3 | Vendor Identification | "STM     " (8字节) | ✅ 静态确认 |
| 4 | Product Identification | "STM32 SRAM Disk " (16字节) | ✅ 静态确认 |

---

## 8. 常见问题排查

### 8.1 编译报错：cannot open source input file "scsi_data.h"

**原因**：`scsi_data.c` 或 `scsi_data.h` 未加入 uvprojx。

**修复**：确认 `project.uvprojx` 的 src Group 已加入两个文件。

### 8.2 Inquiry 产品名长度错误

**原因**：Product Identification 必须恰好 16 字节。手动数字符容易出错。

**修复**：数一遍 `'S','T','M','3','2',' ','S','R','A','M',' ','D','i','s','k',' '` 确认 16 个。

---

## 9. 与下一步的衔接

TR2-A2 完成后，SCSI 静态数据就位。下一步：

- **TR2-A3**：`usb_bot.c/h` BOT 状态机骨架 + `usb_endp.c` 端点回调接管。A3 不直接引用 A2 的数据（SCSI 命令全返回 INVALID），但 A3 的 `usb_bot.h` 需要定义 CBW/CSW 结构体。
- **TR2-A4**：`memory.c/h` 缓冲调度骨架。
- **TR2-B1**：`usb_scsi.c/h` SCSI 查询命令实现。B1 会引用 A2 的 7 个静态数据数组 + A1 的 MAL 接口。

> **A2 是 SCSI 响应的数据基础**：INQUIRY 的产品名、REQUEST_SENSE 的错误信息、READ_CAPACITY 的容量格式都定义在此。B1 实现命令处理时直接引用这些数组。

---

*文档结束。*
