# TR2-B1 详细设计：usb_scsi SCSI 查询命令

## 修改记录

| 版本 | 日期 | 修改内容 | 修改人 |
|---|---|---|---|
| V1.0 | 2026-08-02 | 初始版本 | Copilot |

---

## 文档信息

| 项目 | 内容 |
|---|---|
| 需求编号 | TR2-B1 |
| 需求描述 | `usb_scsi.c/h` SCSI 查询命令实现（INQUIRY/READ_CAPACITY/TEST_UNIT_READY/REQUEST_SENSE/MODE_SENSE6/10/READ_FORMAT_CAPACITIES/START_STOP_UNIT/ALLOW_MEDIUM_REMOVAL）+ `usb_bot.c` CBW_Decode 接入 + Transfer_Data_Request 数据发送 |
| 验收标准 | **Problem Code 10 消失**，USBSTOR + disk.sys 驱动完全加载，PC 显示约 8KB 可移动磁盘（16 块 × 512B） |
| 所属阶段 | TR2-B — SCSI 查询命令走通（纵向切片） |
| 前置文档 | `项目框架设计.md`、`TR2框架设计.md`（§3.2） |
| 前置代码 | TR2-A1 MAL、TR2-A2 scsi_data、TR2-A3 usb_bot 骨架、TR2-A4 memory 骨架均已完成 |
| 完成态参照 | `tmp\Mass_Storage\src\usb_scsi.c`、`tmp\Mass_Storage\inc\usb_scsi.h` |

---

## 1. 需求分解

TR2-B1 是 TR2 阶段的**第一个里程碑**：从"驱动启动失败（Problem Code 10）"到"磁盘可见"。骨架阶段（A1~A4）USBSTOR 驱动发 SCSI 命令时设备全部返回 `CSW_CMD_FAILED`，驱动无法加载磁盘；B1 实现 SCSI 查询命令后，USBSTOR 能正确查询到介质信息，从而加载 disk.sys 并显示磁盘。

**B1 的三件事**：
1. 新建 `usb_scsi.c/h`：实现 9 个 SCSI 查询命令 + `Set_Scsi_Sense_Data` + 不支持命令的统一处理（`SCSI_Invalid_Cmd`/`SCSI_Valid_Cmd` + 宏别名）
2. 修改 `usb_bot.c`：取消 `CBW_Decode()` switch 中查询命令与不支持命令的 `#if 0` 包裹，恢复 4 处被注释的 `Set_Scsi_Sense_Data()` 调用，加回 `#include "usb_scsi.h"`
3. 修改 `memory.c`：恢复 `#include "usb_scsi.h"` 注释（保持与完成态一致）

**与完成态的差异**：完成态的 `usb_scsi.c` 还包含 READ10/WRITE10/VERIFY10/FORMAT_UNIT/ADDRESS_MANAGEMENT（C1~C3 实现）。B1 阶段 `usb_bot.c` 中这 4 个命令的 case 继续用 `#if 0` 包裹，`usb_scsi.c` 不实现对应函数，`usb_scsi.h` 不声明对应函数——**查询命令是 B1 的边界，数据命令留 C1/C2，其余命令留 C3**。

本任务新建 2 个文件，修改 2 个文件：

| 操作 | 文件 | 内容 |
|---|---|---|
| 新建 | `inc/usb_scsi.h` | SCSI 命令宏 + Sense Key/ASC 宏 + 不支持命令宏别名 + 函数声明 |
| 新建 | `src/usb_scsi.c` | 9 个查询命令实现 + Set_Scsi_Sense_Data + Invalid/Valid_Cmd |
| 修改 | `src/usb_bot.c` | 取消 `#if 0` 包裹（查询 + 不支持命令）、恢复 Sense 调用、加 include |
| 修改 | `src/memory.c` | 恢复 `#include "usb_scsi.h"` 注释 |

---

## 2. 关键技术决策

### 2.1 B1 的命令边界：查询命令 vs 数据命令

`CBW_Decode()` 的 switch 共 25 个 case，B1 只恢复 21 个，保留 4 个 `#if 0`：

| 分类 | 命令 | B1 处理 |
|---|---|---|
| 查询命令（9） | REQUEST_SENSE / INQUIRY / START_STOP_UNIT / ALLOW_MEDIUM_REMOVAL / MODE_SENSE6 / MODE_SENSE10 / READ_FORMAT_CAPACITIES / READ_CAPACITY10 / TEST_UNIT_READY | ✅ 恢复 case，usb_scsi.c 实现 |
| 不支持命令（12） | MODE_SELECT10/6 / SEND_DIAGNOSTIC / READ6/12/16 / READ_CAPACITY16 / WRITE6/12/16 / VERIFY12/16 | ✅ 恢复 case，usb_scsi.h 用宏别名映射到 `SCSI_Invalid_Cmd` |
| 数据命令（2） | READ10 / WRITE10 | ⛔ 保持 `#if 0`（C1/C2 恢复） |
| 其他（2） | VERIFY10 / FORMAT_UNIT | ⛔ 保持 `#if 0`（C3 恢复） |

**依据**：USBSTOR 枚举阶段只发查询命令（INQUIRY/READ_CAPACITY10/TEST_UNIT_READY/MODE_SENSE 等），不发 READ10/WRITE10。B1 保留数据命令 case 的 `#if 0`，可避免引用不存在的 `SCSI_Read10_Cmd` 等函数导致链接错误。

同理，`Mass_Storage_In()` 的 `BOT_DATA_IN` 分支和 `Mass_Storage_Out()` 的 `BOT_DATA_OUT` 分支**保持 `#if 0`**（依赖 READ10/WRITE10）。

### 2.2 SCSI 命令宏的重复定义防范

`usb_bot.h`（A3）中已有 `SCSI_READ10`/`SCSI_WRITE10` 的 `#ifndef` 占位（`CBW_Decode()` 解析 LBA/BlkLen 时引用）。B1 创建的 `usb_scsi.h` 定义全部命令宏时，这两个宏同样用 `#ifndef` 守卫，与 `usb_bot.h` 互不冲突（两处值相同 0x28/0x2A）。

### 2.3 数据长度宏与静态数据的归属：复用 scsi_data.h

参考例程把数据长度宏（`MODE_SENSE6_DATA_LEN` 等）和静态数据 extern（`Mode_Sense6_data` 等）定义在 `usb_scsi.h`。**本项目 A2 已把它们定义在 `scsi_data.h`**，B1 的 `usb_scsi.h` 通过 `#include "scsi_data.h"` 复用，**不重复定义**，避免重定义冲突。`usb_scsi.c` 同时包含两个头文件。

### 2.4 `Standard_Inquiry_Data2` 裁剪

参考例程的 `SCSI_Inquiry_Cmd` 对 `lun != 0` 使用 `Standard_Inquiry_Data2`。本项目单 LUN（`MAX_LUN 0`，`scsi_data.h` 无此数组），B1 裁剪该分支：`lun == 0` 直接用 `Standard_Inquiry_Data`，非 0 也回退到标准数据（防御性保留判断）。

### 2.5 `Set_Scsi_Sense_Data` 的位置

`usb_bot.c`（A3）中被注释的 4 处 `Set_Scsi_Sense_Data()` 调用在 B1 恢复。函数实现在 `usb_scsi.c` 提供（参考例程）：

```c
void Set_Scsi_Sense_Data(uint8_t lun, uint8_t Sens_Key, uint8_t Asc)
{
  Scsi_Sense_Data[2]  = Sens_Key;   /* Sense Key */
  Scsi_Sense_Data[12] = Asc;        /* Additional Sense Code */
}
```

`usb_bot.c` 调用的参数宏（`ILLEGAL_REQUEST`/`PARAMETER_LIST_LENGTH_ERROR`/`INVALID_FIELED_IN_COMMAND`/`INVALID_COMMAND`）由 `usb_scsi.h` 提供。

### 2.6 MAL_GetStatus 的可用性

`SCSI_ReadCapacity10_Cmd`/`SCSI_ReadFormatCapacity_Cmd`/`SCSI_TestUnitReady_Cmd` 用 `MAL_GetStatus(lun)` 判断介质是否就绪（A1 已实现，`mass_mal.h` 已声明），B1 直接可用。

### 2.7 工程文件

`project.uvprojx` 的 src Group 加入 `usb_scsi.c`，inc Group 加入 `usb_scsi.h`。

---

## 3. 涉及组件

```
EP2 OUT 收到 CBW (usb_endp.c → Mass_Storage_Out)
    │
    └── CBW_Decode() (usb_bot.c)
        │  恢复的 21 个 case
        ├── 查询命令 ──→ usb_scsi.c (SCSI_Inquiry_Cmd 等 9 个)
        │                   ├── scsi_data.c (静态数据, A2)
        │                   └── mass_mal.c  (MAL_GetStatus, A1)
        ├── 不支持命令 ──→ SCSI_Invalid_Cmd (usb_scsi.c, 宏别名)
        └── (READ10/WRITE10/VERIFY10/FORMAT_UNIT 仍 #if 0, C1~C3)
            │
            └── Transfer_Data_Request() → EP1 IN 发数据 → BOT_DATA_IN_LAST → Set_CSW
```

数据流：USBSTOR 发查询 CBW → `CBW_Decode` 分发 → `usb_scsi.c` 填充响应数据 → `Transfer_Data_Request`（usb_bot.c）发到 EP1 IN → IN 中断 → `Mass_Storage_In` 的 `BOT_DATA_IN_LAST` 分支 → `Set_CSW(CSW_CMD_PASSED)`。

---

## 4. 文件清单

### 4.1 新建文件

| 文件 | 说明 |
|---|---|
| `inc/usb_scsi.h` | SCSI 命令宏 + Sense Key/ASC 宏 + 不支持命令宏别名 + 函数声明 |
| `src/usb_scsi.c` | 9 个查询命令实现 + Set_Scsi_Sense_Data + Invalid/Valid_Cmd |

### 4.2 修改文件

| 文件 | 修改内容 |
|---|---|
| `src/usb_bot.c` | include 区加 `#include "usb_scsi.h"`；switch 的 `#if 0` 拆分为"恢复 21 个 case + 保留 4 个 case `#if 0`"；恢复 4 处 `Set_Scsi_Sense_Data()` 注释调用 |
| `src/memory.c` | 恢复 `#include "usb_scsi.h"` 注释（第 21 行 `/* B1 恢复: ... */` → 实际 include） |
| `project.uvprojx` | src Group 加 `usb_scsi.c`，inc Group 加 `usb_scsi.h` |

### 4.3 不修改的文件

`usb_bot.h`（`#ifndef` 占位继续保留）、`mass_mal.c/h`、`scsi_data.c/h`、`memory.h`、`usb_endp.c`、`usb_conf.h` 在 B1 均不变。

---

## 5. 接口设计

### 5.1 `inc/usb_scsi.h`

基于参考例程裁剪：去掉 `hw_config.h` 依赖（自包含 `stm32f10x.h`）、去掉 `Standard_Inquiry_Data2` extern（单 LUN）、数据长度宏与数据 extern 改为复用 `scsi_data.h`、`SCSI_READ10`/`SCSI_WRITE10` 用 `#ifndef` 守卫、不声明 C1~C3 的函数：

```c
#ifndef __USB_SCSI_H
#define __USB_SCSI_H

#include "stm32f10x.h"
#include "scsi_data.h"    /* 数据长度宏 + 静态数据 extern (A2 定义) */

/* ==== SCSI Commands ==== */
#define SCSI_FORMAT_UNIT               0x04
#define SCSI_INQUIRY                   0x12
#define SCSI_MODE_SELECT6              0x15
#define SCSI_MODE_SELECT10             0x55
#define SCSI_MODE_SENSE6               0x1A
#define SCSI_MODE_SENSE10              0x5A
#define SCSI_ALLOW_MEDIUM_REMOVAL      0x1E
#define SCSI_READ6                     0x08
#ifndef SCSI_READ10                    /* usb_bot.h 已有 #ifndef 占位 */
#define SCSI_READ10                    0x28
#endif
#define SCSI_READ12                    0xA8
#define SCSI_READ16                    0x88
#define SCSI_READ_CAPACITY10           0x25
#define SCSI_READ_CAPACITY16           0x9E
#define SCSI_REQUEST_SENSE             0x03
#define SCSI_START_STOP_UNIT           0x1B
#define SCSI_TEST_UNIT_READY           0x00
#define SCSI_WRITE6                    0x0A
#ifndef SCSI_WRITE10
#define SCSI_WRITE10                   0x2A
#endif
#define SCSI_WRITE12                   0xAA
#define SCSI_WRITE16                   0x8A
#define SCSI_VERIFY10                  0x2F
#define SCSI_VERIFY12                  0xAF
#define SCSI_VERIFY16                  0x8F
#define SCSI_SEND_DIAGNOSTIC           0x1D
#define SCSI_READ_FORMAT_CAPACITIES    0x23

/* ==== Sense Key ==== */
#define NO_SENSE                       0
#define RECOVERED_ERROR                1
#define NOT_READY                      2
#define MEDIUM_ERROR                   3
#define HARDWARE_ERROR                 4
#define ILLEGAL_REQUEST                5
#define UNIT_ATTENTION                 6
#define DATA_PROTECT                   7
#define BLANK_CHECK                    8
#define VENDOR_SPECIFIC                9
#define COPY_ABORTED                   10
#define ABORTED_COMMAND                11
#define VOLUME_OVERFLOW                13
#define MISCOMPARE                     14

/* ==== ASC (Additional Sense Code) ==== */
#define INVALID_COMMAND                0x20
#define INVALID_FIELED_IN_COMMAND      0x24
#define PARAMETER_LIST_LENGTH_ERROR    0x1A
#define INVALID_FIELD_IN_PARAMETER_LIST 0x26
#define ADDRESS_OUT_OF_RANGE           0x21
#define MEDIUM_NOT_PRESENT             0x3A
#define MEDIUM_HAVE_CHANGED            0x28

#define BLKVFY                         0x04   /* VERIFY10 用 (C3), 先定义 */

/* ==== 函数声明 (B1 实现的查询命令) ==== */
void SCSI_Inquiry_Cmd(uint8_t lun);
void SCSI_ReadFormatCapacity_Cmd(uint8_t lun);
void SCSI_ReadCapacity10_Cmd(uint8_t lun);
void SCSI_RequestSense_Cmd(uint8_t lun);
void SCSI_Start_Stop_Unit_Cmd(uint8_t lun);
void SCSI_ModeSense6_Cmd(uint8_t lun);
void SCSI_ModeSense10_Cmd(uint8_t lun);
void SCSI_TestUnitReady_Cmd(uint8_t lun);
void Set_Scsi_Sense_Data(uint8_t lun, uint8_t Sens_Key, uint8_t Asc);

/* 不支持命令的统一处理 */
void SCSI_Invalid_Cmd(uint8_t lun);
void SCSI_Valid_Cmd(uint8_t lun);
#define SCSI_Prevent_Removal_Cmd        SCSI_Valid_Cmd

/* Invalid (Unsupported) commands → 宏别名 */
#define SCSI_READ_CAPACITY16_Cmd        SCSI_Invalid_Cmd
#define SCSI_Write6_Cmd                 SCSI_Invalid_Cmd
#define SCSI_Write12_Cmd                SCSI_Invalid_Cmd
#define SCSI_Write16_Cmd                SCSI_Invalid_Cmd
#define SCSI_Read6_Cmd                  SCSI_Invalid_Cmd
#define SCSI_Read12_Cmd                 SCSI_Invalid_Cmd
#define SCSI_Read16_Cmd                 SCSI_Invalid_Cmd
#define SCSI_Send_Diagnostic_Cmd        SCSI_Invalid_Cmd
#define SCSI_Mode_Select6_Cmd           SCSI_Invalid_Cmd
#define SCSI_Mode_Select10_Cmd          SCSI_Invalid_Cmd
#define SCSI_Verify12_Cmd               SCSI_Invalid_Cmd
#define SCSI_Verify16_Cmd               SCSI_Invalid_Cmd

/* C1~C3 再声明: SCSI_Read10_Cmd / SCSI_Write10_Cmd / SCSI_Verify10_Cmd /
 * SCSI_Format_Cmd / SCSI_Address_Management */

#endif /* __USB_SCSI_H */
```

> **注意**：`SCSI_READ10`/`SCSI_WRITE10` 的 `#ifndef` 守卫保证先包含 `usb_bot.h` 或先包含 `usb_scsi.h` 均不冲突。`CBW_Decode()` 中保留 `#if 0` 的 READ10/WRITE10 case 在 B1 不编译，但宏定义保留无副作用（C1/C2 恢复时直接用）。

### 5.2 `src/usb_scsi.c`

基于参考例程裁剪：`hw_config.h` → `usb_lib.h`、去掉 `Standard_Inquiry_Data2` 分支、去掉 READ10/WRITE10/VERIFY10/FORMAT_UNIT/ADDRESS_MANAGEMENT（C1~C3）、extern 只保留查询命令实际引用的符号：

```c
/**
  * @file    usb_scsi.c
  * @brief   SCSI 查询命令实现 (TR2-B1)
  *
  *          B1 阶段只实现 9 个查询命令 + Set_Scsi_Sense_Data + Invalid/Valid_Cmd。
  *          与完成态差异: READ10/WRITE10 (C1/C2)、VERIFY10/FORMAT_UNIT (C3)、
  *          SCSI_Address_Management (C1) 在后续阶段追加。
  */

/* Includes ------------------------------------------------------------------*/
#include "usb_scsi.h"
#include "scsi_data.h"
#include "mass_mal.h"      /* MAL_GetStatus / Mass_Block_* */
#include "usb_bot.h"       /* CBW/CSW / Bot_Abort / Set_CSW / Transfer_Data_Request */
#include "usb_lib.h"       /* USB 库 (一致性) */

/* Extern variables ----------------------------------------------------------*/
extern Bulk_Only_CBW CBW;
extern Bulk_Only_CSW CSW;
extern uint32_t Mass_Block_Size[2];
extern uint32_t Mass_Block_Count[2];

/* ==== SCSI_Inquiry_Cmd (0x12) ==== */
void SCSI_Inquiry_Cmd(uint8_t lun)
{
  uint8_t *Inquiry_Data;
  uint16_t Inquiry_Data_Length;

  if (CBW.CB[1] & 0x01)      /* EVPD 置位: 返回支持页面列表 */
  {
    Inquiry_Data = Page00_Inquiry_Data;
    Inquiry_Data_Length = 5;
  }
  else
  {
    Inquiry_Data = Standard_Inquiry_Data;   /* 单 LUN, 无 Standard_Inquiry_Data2 */
    if (CBW.CB[4] <= STANDARD_INQUIRY_DATA_LEN)
      Inquiry_Data_Length = CBW.CB[4];
    else
      Inquiry_Data_Length = STANDARD_INQUIRY_DATA_LEN;
  }
  Transfer_Data_Request(Inquiry_Data, Inquiry_Data_Length);
}

/* ==== SCSI_ReadFormatCapacity_Cmd (0x23) ==== */
void SCSI_ReadFormatCapacity_Cmd(uint8_t lun)
{
  if (MAL_GetStatus(lun) != 0)
  {
    Set_Scsi_Sense_Data(CBW.bLUN, NOT_READY, MEDIUM_NOT_PRESENT);
    Set_CSW(CSW_CMD_FAILED, SEND_CSW_ENABLE);
    Bot_Abort(DIR_IN);
    return;
  }
  ReadFormatCapacity_Data[4]  = (uint8_t)(Mass_Block_Count[lun] >> 24);
  ReadFormatCapacity_Data[5]  = (uint8_t)(Mass_Block_Count[lun] >> 16);
  ReadFormatCapacity_Data[6]  = (uint8_t)(Mass_Block_Count[lun] >>  8);
  ReadFormatCapacity_Data[7]  = (uint8_t)(Mass_Block_Count[lun]);
  ReadFormatCapacity_Data[9]  = (uint8_t)(Mass_Block_Size[lun] >>  16);
  ReadFormatCapacity_Data[10] = (uint8_t)(Mass_Block_Size[lun] >>   8);
  ReadFormatCapacity_Data[11] = (uint8_t)(Mass_Block_Size[lun]);
  Transfer_Data_Request(ReadFormatCapacity_Data, READ_FORMAT_CAPACITY_DATA_LEN);
}

/* ==== SCSI_ReadCapacity10_Cmd (0x25) ==== */
void SCSI_ReadCapacity10_Cmd(uint8_t lun)
{
  if (MAL_GetStatus(lun))
  {
    Set_Scsi_Sense_Data(CBW.bLUN, NOT_READY, MEDIUM_NOT_PRESENT);
    Set_CSW(CSW_CMD_FAILED, SEND_CSW_ENABLE);
    Bot_Abort(DIR_IN);
    return;
  }
  /* 最后 LBA (0-based) */
  ReadCapacity10_Data[0] = (uint8_t)((Mass_Block_Count[lun] - 1) >> 24);
  ReadCapacity10_Data[1] = (uint8_t)((Mass_Block_Count[lun] - 1) >> 16);
  ReadCapacity10_Data[2] = (uint8_t)((Mass_Block_Count[lun] - 1) >>  8);
  ReadCapacity10_Data[3] = (uint8_t)(Mass_Block_Count[lun] - 1);
  /* 块长 512 */
  ReadCapacity10_Data[4] = (uint8_t)(Mass_Block_Size[lun] >> 24);
  ReadCapacity10_Data[5] = (uint8_t)(Mass_Block_Size[lun] >> 16);
  ReadCapacity10_Data[6] = (uint8_t)(Mass_Block_Size[lun] >>  8);
  ReadCapacity10_Data[7] = (uint8_t)(Mass_Block_Size[lun]);
  Transfer_Data_Request(ReadCapacity10_Data, READ_CAPACITY10_DATA_LEN);
}

/* ==== SCSI_ModeSense6_Cmd (0x1A) ==== */
void SCSI_ModeSense6_Cmd(uint8_t lun)
{
  Transfer_Data_Request(Mode_Sense6_data, MODE_SENSE6_DATA_LEN);
}

/* ==== SCSI_ModeSense10_Cmd (0x5A) ==== */
void SCSI_ModeSense10_Cmd(uint8_t lun)
{
  Transfer_Data_Request(Mode_Sense10_data, MODE_SENSE10_DATA_LEN);
}

/* ==== SCSI_RequestSense_Cmd (0x03) ==== */
void SCSI_RequestSense_Cmd(uint8_t lun)
{
  uint8_t Request_Sense_data_Length;

  if (CBW.CB[4] <= REQUEST_SENSE_DATA_LEN)
    Request_Sense_data_Length = CBW.CB[4];
  else
    Request_Sense_data_Length = REQUEST_SENSE_DATA_LEN;

  Transfer_Data_Request(Scsi_Sense_Data, Request_Sense_data_Length);
}

/* ==== Set_Scsi_Sense_Data ==== */
void Set_Scsi_Sense_Data(uint8_t lun, uint8_t Sens_Key, uint8_t Asc)
{
  Scsi_Sense_Data[2]  = Sens_Key;   /* Sense Key */
  Scsi_Sense_Data[12] = Asc;        /* Additional Sense Code */
}

/* ==== SCSI_Start_Stop_Unit_Cmd (0x1B, ALLOW_MEDIUM_REMOVAL 0x1E 复用) ==== */
void SCSI_Start_Stop_Unit_Cmd(uint8_t lun)
{
  Set_CSW(CSW_CMD_PASSED, SEND_CSW_ENABLE);
}

/* ==== SCSI_TestUnitReady_Cmd (0x00) ==== */
void SCSI_TestUnitReady_Cmd(uint8_t lun)
{
  if (MAL_GetStatus(lun))
  {
    Set_Scsi_Sense_Data(CBW.bLUN, NOT_READY, MEDIUM_NOT_PRESENT);
    Set_CSW(CSW_CMD_FAILED, SEND_CSW_ENABLE);
    Bot_Abort(DIR_IN);
    return;
  }
  Set_CSW(CSW_CMD_PASSED, SEND_CSW_ENABLE);
}

/* ==== SCSI_Invalid_Cmd (不支持命令统一处理) ==== */
void SCSI_Invalid_Cmd(uint8_t lun)
{
  if (CBW.dDataLength == 0)
  {
    Bot_Abort(DIR_IN);
  }
  else
  {
    if ((CBW.bmFlags & 0x80) != 0)
      Bot_Abort(DIR_IN);
    else
      Bot_Abort(BOTH_DIR);
  }
  Set_Scsi_Sense_Data(CBW.bLUN, ILLEGAL_REQUEST, INVALID_COMMAND);
  Set_CSW(CSW_CMD_FAILED, SEND_CSW_DISABLE);
}

/* ==== SCSI_Valid_Cmd (无数据传输的合法命令) ==== */
void SCSI_Valid_Cmd(uint8_t lun)
{
  if (CBW.dDataLength != 0)
  {
    Bot_Abort(BOTH_DIR);
    Set_Scsi_Sense_Data(CBW.bLUN, ILLEGAL_REQUEST, INVALID_COMMAND);
    Set_CSW(CSW_CMD_FAILED, SEND_CSW_DISABLE);
  }
  else
    Set_CSW(CSW_CMD_PASSED, SEND_CSW_ENABLE);
}
```

> **说明**：`extern uint32_t Mass_Memory_Size[2]` 在查询命令中未引用（仅 C1/C2 的 ADDRESS_MANAGEMENT 使用），B1 不声明；`Bulk_Data_Buff`/`Bot_State` 同理，C1/C2 追加。

### 5.3 `src/usb_bot.c` 修改

#### (a) include 区

```c
#include "usb_lib.h"
#include "usb_bot.h"
#include "mass_mal.h"
#include "usb_scsi.h"   /* B1: SCSI 命令宏 + 查询命令函数声明 */
```

#### (b) `CBW_Decode()` switch 的 `#if 0` 拆分

A3 的 `#if 0` 包裹 25 个 case，B1 改为：21 个 case 恢复编译，4 个数据命令 case 保留独立 `#if 0` 块：

```c
switch (CBW.CB[0])
{
  /* === B1: 查询命令 + 不支持命令 (恢复) === */
  case SCSI_REQUEST_SENSE:       SCSI_RequestSense_Cmd(CBW.bLUN);       break;
  case SCSI_INQUIRY:             SCSI_Inquiry_Cmd(CBW.bLUN);            break;
  case SCSI_START_STOP_UNIT:     SCSI_Start_Stop_Unit_Cmd(CBW.bLUN);    break;
  case SCSI_ALLOW_MEDIUM_REMOVAL: SCSI_Start_Stop_Unit_Cmd(CBW.bLUN);   break;
  case SCSI_MODE_SENSE6:         SCSI_ModeSense6_Cmd(CBW.bLUN);         break;
  case SCSI_MODE_SENSE10:        SCSI_ModeSense10_Cmd(CBW.bLUN);        break;
  case SCSI_READ_FORMAT_CAPACITIES: SCSI_ReadFormatCapacity_Cmd(CBW.bLUN); break;
  case SCSI_READ_CAPACITY10:     SCSI_ReadCapacity10_Cmd(CBW.bLUN);     break;
  case SCSI_TEST_UNIT_READY:     SCSI_TestUnitReady_Cmd(CBW.bLUN);      break;
  /* 不支持命令 (宏别名 → SCSI_Invalid_Cmd) */
  case SCSI_MODE_SELECT10:       SCSI_Mode_Select10_Cmd(CBW.bLUN);      break;
  case SCSI_MODE_SELECT6:        SCSI_Mode_Select6_Cmd(CBW.bLUN);       break;
  case SCSI_SEND_DIAGNOSTIC:     SCSI_Send_Diagnostic_Cmd(CBW.bLUN);    break;
  case SCSI_READ6:               SCSI_Read6_Cmd(CBW.bLUN);              break;
  case SCSI_READ12:              SCSI_Read12_Cmd(CBW.bLUN);             break;
  case SCSI_READ16:              SCSI_Read16_Cmd(CBW.bLUN);             break;
  case SCSI_READ_CAPACITY16:     SCSI_READ_CAPACITY16_Cmd(CBW.bLUN);    break;
  case SCSI_WRITE6:              SCSI_Write6_Cmd(CBW.bLUN);             break;
  case SCSI_WRITE12:             SCSI_Write12_Cmd(CBW.bLUN);            break;
  case SCSI_WRITE16:             SCSI_Write16_Cmd(CBW.bLUN);            break;
  case SCSI_VERIFY12:            SCSI_Verify12_Cmd(CBW.bLUN);           break;
  case SCSI_VERIFY16:            SCSI_Verify16_Cmd(CBW.bLUN);           break;

#if 0  /* === 数据命令 (C1/C2/C3 恢复) === */
  case SCSI_READ10:              SCSI_Read10_Cmd(CBW.bLUN, SCSI_LBA, SCSI_BlkLen); break;
  case SCSI_WRITE10:             SCSI_Write10_Cmd(CBW.bLUN, SCSI_LBA, SCSI_BlkLen); break;
  case SCSI_VERIFY10:            SCSI_Verify10_Cmd(CBW.bLUN);           break;
  case SCSI_FORMAT_UNIT:         SCSI_Format_Cmd(CBW.bLUN);             break;
#endif

  default:
    Bot_Abort(BOTH_DIR);
    Set_Scsi_Sense_Data(CBW.bLUN, ILLEGAL_REQUEST, INVALID_COMMAND);   /* B1: 恢复 */
    Set_CSW(CSW_CMD_FAILED, SEND_CSW_DISABLE);
    break;
}
```

#### (c) 恢复 4 处 `Set_Scsi_Sense_Data()` 调用

| 位置 | 恢复内容 |
|---|---|
| `Data_Len != BOT_CBW_PACKET_LENGTH` 分支 | `Set_Scsi_Sense_Data(CBW.bLUN, ILLEGAL_REQUEST, PARAMETER_LIST_LENGTH_ERROR);` |
| `bLUN/bCBLength` 非法分支 | `Set_Scsi_Sense_Data(CBW.bLUN, ILLEGAL_REQUEST, INVALID_FIELED_IN_COMMAND);` |
| switch `default` 分支 | `Set_Scsi_Sense_Data(CBW.bLUN, ILLEGAL_REQUEST, INVALID_COMMAND);` |
| CBW signature 非法分支 | `Set_Scsi_Sense_Data(CBW.bLUN, ILLEGAL_REQUEST, INVALID_COMMAND);` |

**保持不变**：`Mass_Storage_In()` 的 `BOT_DATA_IN` 分支、`Mass_Storage_Out()` 的 `BOT_DATA_OUT` 分支仍为 `#if 0`（C1/C2 恢复）。

### 5.4 `src/memory.c` 修改

恢复 include（第 21 行注释）：

```c
/* 改前 */
/* B1 恢复: #include "usb_scsi.h" */

/* 改后 */
#include "usb_scsi.h"   /* B1: SCSI 宏/函数声明 (保持与完成态一致) */
```

**注意**：`Read_Memory`/`Write_Memory` 中的 `MAL_Read`/`MAL_Write` 调用继续 `#if 0`（C1/C2 恢复），B1 不触碰。

---

## 6. 实现要点与风险

### 6.1 B1 的可观测变化（A4 → B1）

| 项 | A4 | B1 | 变化 |
|---|---|---|---|
| Problem Code | 10 | **消失** | USBSTOR 查询命令全部正确响应 |
| 设备管理器 → 磁盘驱动器 | 无 | 出现 STM32 SRAM Disk | disk.sys 加载成功 |
| 磁盘容量 | — | 约 8KB（16 块 × 512B） | READ_CAPACITY10 响应 |
| 格式化 | — | **无法格式化**（预期） | WRITE10 未实现，C2 恢复 |

### 6.2 USBSTOR 枚举阶段的关键命令序列

USBSTOR 加载磁盘通常依次发出：`INQUIRY` → `TEST_UNIT_READY` → `REQUEST_SENSE` → `READ_CAPACITY10` → `MODE_SENSE6/10` → `READ_FORMAT_CAPACITIES`。**任何一个返回 FAILED 都可能让 Problem Code 10 保留**。B1 实现务必按 §5.2 完整实现，`MAL_GetStatus` 返回 0（MAL_OK）保证 `TEST_UNIT_READY`/`READ_CAPACITY10` 走成功路径。

### 6.3 风险：MODE_SENSE 数据长度

`Mode_Sense6_data[4]`/`Mode_Sense10_data[8]` 由 A2 提供静态内容（头部字节需符合 Mode Sense 格式），`Transfer_Data_Request` 发送后 `CSW.dDataResidue` 自动扣除。若 Windows 要求更长数据，可能触发额外的 `REQUEST_SENSE` 查询——USBSTOR 会根据 `Scsi_Sense_Data` 判断。A2 数据已按例程原样拷贝，风险低。

### 6.4 风险：`Standard_Inquiry_Data` 的厂商/产品字段

`Standard_Inquiry_Data`（A2）含 "STM " / "STM32 SRAM Disk " / "1.0 "。Windows 设备管理器显示名称取自该字段。若显示异常，检查 A2 数据而非 B1 代码。

### 6.5 链接风险

`usb_scsi.c` 加入工程后，若 `usb_bot.c` 恢复了引用 `SCSI_Read10_Cmd` 等未实现函数的 case，会产生 `undefined symbol`。B1 必须严格按 §2.1 边界：**只恢复 21 个 case**。

---

## 7. 验证流程

### 7.1 编译验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | Keil Build (F7) | 0 Error(s), 0 Warning(s) | 待验证 |

### 7.2 USBTreeView / 设备管理器验证（框架 §8.2）

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 烧录，插 USB | **Problem Code 消失** | TR2-B1 ✅ |
| 2 | USBTreeView String Descriptors | 完整可读 | TR2-B1 ✅ |
| 3 | 设备管理器 → 磁盘驱动器 | 出现 "STM32 SRAM Disk USB Device" | TR2-B1 ✅ |
| 4 | 查看磁盘容量 | 约 8KB（16 块 × 512B） | TR2-B1 ✅ |
| 5 | 尝试格式化 | 失败或不可用（预期，WRITE10 未实现） | B1 边界确认 |

### 7.3 链接验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 查看 `.map` 文件 | `SCSI_Inquiry_Cmd` 等 9 个函数来自 `usb_scsi.o` | 待验证 |

---

## 8. 常见问题排查

### 8.1 Problem Code 10 仍存在

**原因**：某个查询命令响应异常（FAILED/超时/数据格式错），USBSTOR 中断加载。

**排查**：USB 抓包（BusHound/USBlyzer）查看 USBSTOR 发出的命令序列，逐个对照 §6.2 列表检查对应函数是否正确调用 `Transfer_Data_Request` 且返回 `CSW_CMD_PASSED`。重点检查 `MAL_GetStatus` 返回值（必须 0）。

### 8.2 编译报错：undefined symbol SCSI_Read10_Cmd / SCSI_Write10_Cmd

**原因**：`usb_bot.c` 恢复了 READ10/WRITE10 case，但 `usb_scsi.c` 未实现。

**修复**：确认 READ10/WRITE10/VERIFY10/FORMAT_UNIT 4 个 case 保留在 `#if 0` 内（§5.3b）。

### 8.3 编译报错：SCSI_READ10 / SCSI_WRITE10 重复定义

**原因**：`usb_bot.h` 与 `usb_scsi.h` 同时定义且无守卫。

**修复**：确认两处均使用 `#ifndef` 守卫（§5.1）。本项目 `usb_bot.h`（A3）已带守卫。

### 8.4 编译报错：MODE_SENSE6_DATA_LEN 等重复定义

**原因**：`usb_scsi.h` 重复定义了 `scsi_data.h` 已有的长度宏。

**修复**：`usb_scsi.h` 只 `#include "scsi_data.h"`，不重复定义（§2.3）。

### 8.5 磁盘出现但容量显示为 0

**原因**：`READ_CAPACITY10` 返回 0 块（`Mass_Block_Count` 未初始化或读取错位）。

**修复**：确认 `mass_mal.c`（A1）在 `MAL_Init` 中正确填充 `Mass_Block_Count[0] = 16`、`Mass_Block_Size[0] = 512`；`SCSI_ReadCapacity10_Cmd` 的字节序（大端）正确。

### 8.6 链接报错：undefined symbol Set_Scsi_Sense_Data

**原因**：`usb_bot.c` 恢复了 4 处 Sense 调用，但 `usb_scsi.c` 未加入工程或未实现该函数。

**修复**：确认 `usb_scsi.c` 已加入 `project.uvprojx` 且实现 `Set_Scsi_Sense_Data`。

---

## 9. 与下一步的衔接

TR2-B1 完成后，**Problem Code 10 消失**，PC 显示 8KB 可移动磁盘，这是 TR2 的第一个里程碑。后续：

- **TR2-C1**：`usb_scsi.c` 实现 `SCSI_Read10_Cmd`/`SCSI_Address_Management`，`memory.c` 恢复 `MAL_Read`，`usb_bot.c` 恢复 READ10 case 与 `Mass_Storage_In` 的 `BOT_DATA_IN` 分支 → 可读取磁盘内容（0xFF/MBR/FAT）
- **TR2-C2**：`usb_scsi.c` 实现 `SCSI_Write10_Cmd`，`memory.c` 恢复 `MAL_Write`，`usb_bot.c` 恢复 WRITE10 case 与 `Mass_Storage_Out` 的 `BOT_DATA_OUT` 分支 → 可格式化/读写文件
- **TR2-C3**：`VERIFY10`/`FORMAT_UNIT` 等命令补全 + 安全弹出验证

> **B1 是 TR2 的分水岭**：BOT 四层（endp/bot/scsi/buffer/mal）从"骨架可编译"进入"查询命令可响应"。数据路径（READ10/WRITE10）的框架已在 A4 的 memory.c 中就位，C1/C2 只需恢复 `#if 0` 并实现 SCSI 函数。

---

*文档结束。*

