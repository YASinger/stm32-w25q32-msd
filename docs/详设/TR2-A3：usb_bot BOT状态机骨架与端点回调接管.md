# TR2-A3 详细设计：usb_bot BOT 状态机骨架 + usb_endp 端点回调接管

## 修改记录

| 版本 | 日期 | 修改内容 | 修改人 |
|---|---|---|---|
| V1.0 | 2026-07-30 | 初始版本 | Copilot |
| V1.1 | 2026-07-31 | 补充 §5.5 usb_istr.h/c 修改（EP 回调原型声明遗漏）、§8.4 排查 | Copilot |

---

## 文档信息

| 项目 | 内容 |
|---|---|
| 需求编号 | TR2-A3 |
| 需求描述 | `usb_bot.c/h` BOT 状态机骨架 + `usb_endp.c` 端点回调接管（CBW 解码 + CSW 返回，SCSI 全 INVALID） |
| 验收标准 | EP1/EP2 回调不再是 NOP，CBW 能被接收，CSW 能被返回 |
| 所属阶段 | TR2-A — BOT 协议栈骨架可编译运行 |
| 前置文档 | `项目框架设计.md`、`TR2框架设计.md` |
| 前置代码 | TR2-A1 MAL 已完成；TR2-A2 scsi_data 已完成 |
| 完成态参照 | `tmp\Mass_Storage\src\usb_bot.c`、`tmp\Mass_Storage\inc\usb_bot.h`、`tmp\Mass_Storage\src\usb_endp.c` |

---

## 1. 需求分解

TR2-A3 是 TR2-A 骨架阶段的核心步骤，目标是让 EP1/EP2 回调从 `NOP_Process` 替换为真实 BOT 函数，CBW 能被接收并返回 CSW（即使 CSW 说"命令不支持"）。

**A3 的四件事**：
1. 新建 `usb_bot.c/h`：CBW/CSW 结构体、BOT 状态机、`Mass_Storage_In/Out()`、`CBW_Decode()`、`Set_CSW()`、`Transfer_Data_Request()`、`Bot_Abort()`
2. 新建 `usb_endp.c`：`EP1_IN_Callback()` → `Mass_Storage_In()`，`EP2_OUT_Callback()` → `Mass_Storage_Out()`
3. 修改 `usb_conf.h`：注释掉 `EP1_IN_Callback` 和 `EP2_OUT_Callback` 的 `NOP_Process` 宏
4. 修改 `usb_istr.h`/`usb_istr.c`：补入 `EP1_IN_Callback`/`EP2_OUT_Callback` 函数原型声明，并让 `usb_istr.c` 包含 `usb_istr.h`（见 §5.5）

**与完成态的差异**：完成态的 `CBW_Decode()` switch 中调用 `SCSI_Inquiry_Cmd`/`SCSI_Read10_Cmd` 等函数（B1 实现）。A3 阶段这些函数不存在，用 `#if 0` 包裹 SCSI 命令调用，只保留 `default` 分支返回 `CSW_CMD_FAILED`。同理 `Mass_Storage_In()` 的 `BOT_DATA_IN` 分支和 `Mass_Storage_Out()` 的 `BOT_DATA_OUT` 分支也用 `#if 0` 包裹。

本任务新建 3 个文件，修改 1 个文件：

| 操作 | 文件 | 内容 |
|---|---|---|
| 新建 | `inc/usb_bot.h` | CBW/CSW 结构体 + 状态枚举 + 常量 + 函数声明 |
| 新建 | `src/usb_bot.c` | BOT 状态机骨架（CBW 解码 + CSW 返回，SCSI 全 INVALID） |
| 新建 | `src/usb_endp.c` | EP1_IN_Callback + EP2_OUT_Callback |
| 修改 | `inc/usb_conf.h` | 注释掉 EP1_IN_Callback 和 EP2_OUT_Callback 宏 |
| 修改 | `inc/usb_istr.h` | 补入 EP1_IN_Callback / EP2_OUT_Callback 函数原型 |
| 修改 | `src/usb_istr.c` | 加入 `#include "usb_istr.h"` 以获取上述原型 |

---

## 2. 关键技术决策

### 2.1 EP1/EP2 回调切换机制（§6.2）

TR1 阶段 `usb_conf.h` 中：
```c
#define EP1_IN_Callback   NOP_Process
#define EP2_OUT_Callback  NOP_Process
```

TR2 需要替换为真实函数。方法：注释掉这两个宏，在 `usb_endp.c` 中定义同名函数：

```c
/* usb_conf.h */
//#define  EP1_IN_Callback   NOP_Process   /* TR2: 替换为 usb_endp.c 中的真实函数 */
//#define  EP2_OUT_Callback  NOP_Process   /* TR2: 替换为 usb_endp.c 中的真实函数 */
```

`usb_istr.c` 的 `pEpInt_IN[0]` / `pEpInt_OUT[0]` 初始化为 `EP1_IN_Callback` / `EP2_OUT_Callback`——宏被注释后，这两个符号变成未定义的函数名，由 `usb_endp.c` 提供函数定义。

### 2.2 CBW/CSW 结构体

**CBW（Command Block Wrapper，31 字节）**：
```c
typedef struct {
    uint32_t dSignature;      /* 0x43425355 = "USBC" */
    uint32_t dTag;            /* 主机生成, CSW 原样返回 */
    uint32_t dDataLength;     /* 数据阶段总字节数 */
    uint8_t  bmFlags;         /* bit7: 0=OUT, 1=IN */
    uint8_t  bLUN;            /* 逻辑单元号 */
    uint8_t  bCBLength;       /* 命令块长度 (1~16) */
    uint8_t  CB[16];          /* SCSI 命令块 */
} Bulk_Only_CBW;              /* 共 31 字节 */
```

**CSW（Command Status Wrapper，13 字节）**：
```c
typedef struct {
    uint32_t dSignature;      /* 0x53425355 = "USBS" */
    uint32_t dTag;            /* 原样返回 CBW 的 dTag */
    uint32_t dDataResidue;    /* 剩余未传输字节数 */
    uint8_t  bStatus;         /* 0=Passed, 1=Failed, 2=Phase Error */
} Bulk_Only_CSW;              /* 共 13 字节 */
```

### 2.3 BOT 状态机

```
BOT_IDLE ──CBW──→ CBW_Decode() ──┬── 查询命令 ──→ Transfer_Data_Request ──→ BOT_DATA_IN_LAST
                                  ├── READ10 ──→ Read_Memory ──→ BOT_DATA_IN
                                  └── WRITE10 ──→ BOT_DATA_OUT

BOT_DATA_IN_LAST ──IN中断──→ Set_CSW ──→ BOT_CSW_Send ──IN中断──→ BOT_IDLE
BOT_CSW_Send ──IN中断──→ BOT_IDLE
BOT_ERROR ──IN中断──→ BOT_IDLE
```

A3 阶段只走 `CBW_Decode → default → CSW_CMD_FAILED` 路径。

### 2.4 A3 的 `#if 0` 裁剪

完成态的 `CBW_Decode()` switch 有 24 个 case 调用 SCSI 函数。A3 阶段 `usb_scsi.c` 不存在，这些调用用 `#if 0` 包裹，只保留 `default` 分支：

```c
switch (CBW.CB[0]) {
#if 0  /* === SCSI 命令处理 (B1 恢复) === */
    case SCSI_REQUEST_SENSE:  SCSI_RequestSense_Cmd(CBW.bLUN);  break;
    case SCSI_INQUIRY:        SCSI_Inquiry_Cmd(CBW.bLUN);       break;
    ... (全部 case)
#endif
    default:
        Bot_Abort(BOTH_DIR);
        Set_CSW(CSW_CMD_FAILED, SEND_CSW_DISABLE);
}
```

同理，`Mass_Storage_In()` 的 `BOT_DATA_IN` 分支和 `Mass_Storage_Out()` 的 `BOT_DATA_OUT` 分支也用 `#if 0` 包裹。

### 2.5 `BULK_MAX_PACKET_SIZE` 宏

`usb_bot.c` 引用 `BULK_MAX_PACKET_SIZE`（64）定义 `Bulk_Data_Buff`。参考例程在 `hw_config.h` 中定义。本项目的 `hw_config.h`（A2 版本）没有此宏，需要在 `usb_bot.h` 或 `usb_conf.h` 中补入。

**处理**：在 `usb_bot.h` 中定义 `#define BULK_MAX_PACKET_SIZE 0x40`（64）。

### 2.6 `usb_bot.c` 的依赖分析

| 依赖项 | 来源 | A3 是否可用 |
|---|---|---|
| `USB_SIL_Read`/`USB_SIL_Write` | USB 库 `usb_sil.c` | ✅ TR1 已集成 |
| `SetEPTxStatus`/`SetEPRxStatus` | USB 库 `usb_regs.h` | ✅ TR1 已集成 |
| `BULK_MAX_PACKET_SIZE` | `usb_bot.h`（本任务定义） | ✅ |
| `Set_Scsi_Sense_Data` | `usb_scsi.c`（B1） | ❌ `#if 0` 包裹 |
| `SCSI_*_Cmd` | `usb_scsi.c`（B1） | ❌ `#if 0` 包裹 |
| `Read_Memory`/`Write_Memory` | `memory.c`（A4） | ❌ `#if 0` 包裹 |

`Set_Scsi_Sense_Data` 在 `CBW_Decode()` 的错误处理中被调用（如 `CBW.dSignature` 不匹配时）。A3 阶段需要保留这些错误处理，但 `Set_Scsi_Sense_Data` 不存在。

**处理**：A3 阶段 `CBW_Decode()` 中的 `Set_Scsi_Sense_Data()` 调用也用 `#if 0` 包裹，或改为注释。错误处理仍返回 `CSW_CMD_FAILED`，只是不设置 Sense Data。

---

## 3. 涉及组件

```
USB 中断 (usb_istr.c, TR1-A4)
    │
    ├── EP1 IN (发送完成)
    │   └── pEpInt_IN[0] = EP1_IN_Callback (usb_endp.c)
    │       └── Mass_Storage_In() (usb_bot.c)
    │
    └── EP2 OUT (接收完成)
        └── pEpInt_OUT[0] = EP2_OUT_Callback (usb_endp.c)
            └── Mass_Storage_Out() (usb_bot.c)
                └── CBW_Decode() (usb_bot.c)
                    └── default → Set_CSW(CSW_CMD_FAILED)
```

---

## 4. 文件清单

### 4.1 新建文件

| 文件 | 说明 |
|---|---|
| `inc/usb_bot.h` | CBW/CSW 结构体 + 状态枚举 + 常量 + 函数声明 |
| `src/usb_bot.c` | BOT 状态机骨架 |
| `src/usb_endp.c` | EP1_IN_Callback + EP2_OUT_Callback |

### 4.2 修改文件

| 文件 | 修改内容 |
|---|---|
| `inc/usb_conf.h` | 注释掉 `EP1_IN_Callback` 和 `EP2_OUT_Callback` 宏 |
| `inc/usb_istr.h` | 补入 `EP1_IN_Callback` / `EP2_OUT_Callback` 函数原型 |
| `src/usb_istr.c` | 加入 `#include "usb_istr.h"` |

### 4.3 工程分组

`project.uvprojx` 的 `src` Group 加入 `usb_bot.c` 和 `usb_endp.c`，`inc` 加入 `usb_bot.h`。

---

## 5. 接口设计

### 5.1 `inc/usb_bot.h`

与完成态基本一致，补入 `BULK_MAX_PACKET_SIZE` 宏：

```c
#ifndef __USB_BOT_H
#define __USB_BOT_H

#include "stm32f10x.h"

/* Bulk-Only 包大小 */
#define BULK_MAX_PACKET_SIZE   0x40    /* 64 字节 */

/* CBW / CSW 结构体 */
typedef struct {
    uint32_t dSignature;
    uint32_t dTag;
    uint32_t dDataLength;
    uint8_t  bmFlags;
    uint8_t  bLUN;
    uint8_t  bCBLength;
    uint8_t  CB[16];
} Bulk_Only_CBW;

typedef struct {
    uint32_t dSignature;
    uint32_t dTag;
    uint32_t dDataResidue;
    uint8_t  bStatus;
} Bulk_Only_CSW;

/* 状态机 */
#define BOT_IDLE          0
#define BOT_DATA_OUT      1
#define BOT_DATA_IN       2
#define BOT_DATA_IN_LAST  3
#define BOT_CSW_Send      4
#define BOT_ERROR         5

/* 签名 */
#define BOT_CBW_SIGNATURE  0x43425355
#define BOT_CSW_SIGNATURE  0x53425355
#define BOT_CBW_PACKET_LENGTH  31
#define CSW_DATA_LENGTH    0x000D

/* CSW 状态 */
#define CSW_CMD_PASSED     0x00
#define CSW_CMD_FAILED     0x01
#define CSW_PHASE_ERROR    0x02

#define SEND_CSW_DISABLE   0
#define SEND_CSW_ENABLE    1

#define DIR_IN             0
#define DIR_OUT            1
#define BOTH_DIR           2

/* 函数声明 */
void Mass_Storage_In(void);
void Mass_Storage_Out(void);
void CBW_Decode(void);
void Transfer_Data_Request(uint8_t *Data_Pointer, uint16_t Data_Len);
void Set_CSW(uint8_t CSW_Status, uint8_t Send_Permission);
void Bot_Abort(uint8_t Direction);

#endif /* __USB_BOT_H */
```

### 5.2 `src/usb_endp.c`

与完成态完全一致：

```c
#include "usb_lib.h"
#include "usb_bot.h"

void EP1_IN_Callback(void)
{
    Mass_Storage_In();
}

void EP2_OUT_Callback(void)
{
    Mass_Storage_Out();
}
```

### 5.3 `src/usb_bot.c`

骨架版，SCSI 命令调用用 `#if 0` 包裹：

```c
#include "usb_lib.h"
#include "usb_bot.h"

/* 全局变量 */
uint8_t  Bot_State;
uint8_t  Bulk_Data_Buff[BULK_MAX_PACKET_SIZE];
uint16_t Data_Len;
Bulk_Only_CBW CBW;
Bulk_Only_CSW CSW;
uint32_t SCSI_LBA, SCSI_BlkLen;

void Mass_Storage_In(void)
{
    switch (Bot_State)
    {
    case BOT_CSW_Send:
    case BOT_ERROR:
        Bot_State = BOT_IDLE;
        SetEPRxStatus(ENDP2, EP_RX_VALID);
        break;

#if 0  /* === BOT_DATA_IN 分支依赖 SCSI_Read10_Cmd (B1/C1 恢复) === */
    case BOT_DATA_IN:
        switch (CBW.CB[0]) {
        case SCSI_READ10:
            SCSI_Read10_Cmd(CBW.bLUN, SCSI_LBA, SCSI_BlkLen);
            break;
        }
        break;
#endif

    case BOT_DATA_IN_LAST:
        Set_CSW(CSW_CMD_PASSED, SEND_CSW_ENABLE);
        SetEPRxStatus(ENDP2, EP_RX_VALID);
        break;

    default:
        break;
    }
}

void Mass_Storage_Out(void)
{
    Data_Len = USB_SIL_Read(EP2_OUT, Bulk_Data_Buff);

    switch (Bot_State)
    {
    case BOT_IDLE:
        CBW_Decode();
        break;

#if 0  /* === BOT_DATA_OUT 分支依赖 SCSI_Write10_Cmd (B1/C2 恢复) === */
    case BOT_DATA_OUT:
        if (CBW.CB[0] == SCSI_WRITE10) {
            SCSI_Write10_Cmd(CBW.bLUN, SCSI_LBA, SCSI_BlkLen);
            break;
        }
        Bot_Abort(DIR_OUT);
        Set_CSW(CSW_PHASE_ERROR, SEND_CSW_DISABLE);
        break;
#endif

    default:
        Bot_Abort(BOTH_DIR);
        Set_CSW(CSW_PHASE_ERROR, SEND_CSW_DISABLE);
        break;
    }
}

void CBW_Decode(void)
{
    uint32_t Counter;

    for (Counter = 0; Counter < Data_Len; Counter++) {
        *((uint8_t *)&CBW + Counter) = Bulk_Data_Buff[Counter];
    }
    CSW.dTag = CBW.dTag;
    CSW.dDataResidue = CBW.dDataLength;

    if (Data_Len != BOT_CBW_PACKET_LENGTH) {
        Bot_Abort(BOTH_DIR);
        CBW.dSignature = 0;
        Set_CSW(CSW_CMD_FAILED, SEND_CSW_DISABLE);
        return;
    }

    if ((CBW.CB[0] == SCSI_READ10) || (CBW.CB[0] == SCSI_WRITE10)) {
        SCSI_LBA = (CBW.CB[2] << 24) | (CBW.CB[3] << 16) | (CBW.CB[4] << 8) | CBW.CB[5];
        SCSI_BlkLen = (CBW.CB[7] << 8) | CBW.CB[8];
    }

    if (CBW.dSignature == BOT_CBW_SIGNATURE) {
        if ((CBW.bLUN > MAX_LUN) || (CBW.bCBLength < 1) || (CBW.bCBLength > 16)) {
            Bot_Abort(BOTH_DIR);
            Set_CSW(CSW_CMD_FAILED, SEND_CSW_DISABLE);
        } else {
            switch (CBW.CB[0]) {
#if 0  /* === SCSI 命令分发 (B1 恢复) === */
            case SCSI_REQUEST_SENSE:       SCSI_RequestSense_Cmd(CBW.bLUN);       break;
            case SCSI_INQUIRY:             SCSI_Inquiry_Cmd(CBW.bLUN);            break;
            case SCSI_START_STOP_UNIT:     SCSI_Start_Stop_Unit_Cmd(CBW.bLUN);    break;
            case SCSI_ALLOW_MEDIUM_REMOVAL: SCSI_Start_Stop_Unit_Cmd(CBW.bLUN);  break;
            case SCSI_MODE_SENSE6:         SCSI_ModeSense6_Cmd(CBW.bLUN);         break;
            case SCSI_MODE_SENSE10:        SCSI_ModeSense10_Cmd(CBW.bLUN);        break;
            case SCSI_READ_FORMAT_CAPACITIES: SCSI_ReadFormatCapacity_Cmd(CBW.bLUN); break;
            case SCSI_READ_CAPACITY10:     SCSI_ReadCapacity10_Cmd(CBW.bLUN);     break;
            case SCSI_TEST_UNIT_READY:     SCSI_TestUnitReady_Cmd(CBW.bLUN);      break;
            case SCSI_READ10:              SCSI_Read10_Cmd(CBW.bLUN, SCSI_LBA, SCSI_BlkLen);  break;
            case SCSI_WRITE10:             SCSI_Write10_Cmd(CBW.bLUN, SCSI_LBA, SCSI_BlkLen); break;
            case SCSI_VERIFY10:            SCSI_Verify10_Cmd(CBW.bLUN);           break;
            case SCSI_FORMAT_UNIT:         SCSI_Format_Cmd(CBW.bLUN);             break;
#endif
            default:
                Bot_Abort(BOTH_DIR);
                Set_CSW(CSW_CMD_FAILED, SEND_CSW_DISABLE);
                break;
            }
        }
    } else {
        Bot_Abort(BOTH_DIR);
        Set_CSW(CSW_CMD_FAILED, SEND_CSW_DISABLE);
    }
}

void Transfer_Data_Request(uint8_t *Data_Pointer, uint16_t Data_Len)
{
    USB_SIL_Write(EP1_IN, Data_Pointer, Data_Len);
    SetEPTxStatus(ENDP1, EP_TX_VALID);
    Bot_State = BOT_DATA_IN_LAST;
    CSW.dDataResidue -= Data_Len;
    CSW.bStatus = CSW_CMD_PASSED;
}

void Set_CSW(uint8_t CSW_Status, uint8_t Send_Permission)
{
    CSW.dSignature = BOT_CSW_SIGNATURE;
    CSW.bStatus = CSW_Status;
    USB_SIL_Write(EP1_IN, ((uint8_t *)&CSW), CSW_DATA_LENGTH);
    Bot_State = BOT_ERROR;
    if (Send_Permission) {
        Bot_State = BOT_CSW_Send;
        SetEPTxStatus(ENDP1, EP_TX_VALID);
    }
}

void Bot_Abort(uint8_t Direction)
{
    switch (Direction) {
    case DIR_IN:
        SetEPTxStatus(ENDP1, EP_TX_STALL);
        break;
    case DIR_OUT:
        SetEPRxStatus(ENDP2, EP_RX_STALL);
        break;
    case BOTH_DIR:
        SetEPTxStatus(ENDP1, EP_TX_STALL);
        SetEPRxStatus(ENDP2, EP_RX_STALL);
        break;
    default:
        break;
    }
}
```

**与完成态的差异**：

| 差异点 | 完成态 | A3（当前） | 恢复时机 |
|---|---|---|---|
| `#include "usb_scsi.h"` | 有 | 删除 | B1 |
| `#include "memory.h"` | 有 | 删除 | A4 |
| `#include "hw_config.h"` | 有 | 删除（用 `usb_lib.h` 替代） | — |
| `CBW_Decode()` SCSI 命令分发 | 全部编译 | `#if 0` 包裹 | B1 |
| `Mass_Storage_In()` BOT_DATA_IN | 编译 | `#if 0` 包裹 | C1 |
| `Mass_Storage_Out()` BOT_DATA_OUT | 编译 | `#if 0` 包裹 | C2 |
| `Set_Scsi_Sense_Data()` 调用 | 有 | 删除（注释掉） | B1 |
| `extern uint32_t Max_Lun` | 有 | 用 `MAX_LUN` 宏替代 | — |

### 5.4 `inc/usb_conf.h` 修改

注释掉两个回调宏：

```c
/* 改前 */
#define  EP1_IN_Callback   NOP_Process
#define  EP2_OUT_Callback  NOP_Process
/* 改后 */
//#define  EP1_IN_Callback   NOP_Process   /* TR2: 替换为 usb_endp.c 中的真实函数 */
//#define  EP2_OUT_Callback  NOP_Process   /* TR2: 替换为 usb_endp.c 中的真实函数 */
```

### 5.5 `inc/usb_istr.h` 与 `src/usb_istr.c` 修改

> **V1.1 增补**：本节为 V1.0 遗漏项。注释 `usb_conf.h` 宏后，`usb_istr.c` 编译期会丢失 `EP1_IN_Callback`/`EP2_OUT_Callback` 的符号声明。

**背景**：`usb_istr.c` 的 `pEpInt_IN[7]` / `pEpInt_OUT[7]` 数组直接引用 14 个 `EPx_IN/OUT_Callback` 符号。这些符号的可见性来自包含链：

```
usb_istr.c → usb_lib.h → usb_type.h:44 → usb_conf.h
```

- **TR1 时**：`usb_conf.h` 中 `#define EP1_IN_Callback NOP_Process` 等宏经此链被 `usb_istr.c` 看到，`pEpInt_IN[0]` 等价于 `NOP_Process`，无需函数原型即可编译
- **A3 注释宏后**：`EP1_IN_Callback` / `EP2_OUT_Callback` 既不是宏、也无函数原型 → 编译期 `undefined identifier`，链接期才由 `usb_endp.c` 解析已经太晚

**参考例程做法**：`tmp/Mass_Storage/inc/usb_istr.h` 第 55-69 行声明了全部 14 个 EP 回调原型。本项目 TR1-A4 的 `usb_istr.h` 是精简版（只声明 `USB_Istr`），TR1 时因有宏而不需要原型，注释宏前必须先补原型。

**处理**：只声明被接管的两个回调（其余 12 个仍是 `NOP_Process` 宏，无需声明；后续接管更多端点时在此追加）。

`inc/usb_istr.h`：
```c
void USB_Istr(void);

/* TR2-A3: usb_conf.h 中 EP1_IN_Callback / EP2_OUT_Callback 的 NOP_Process
 * 宏已注释，改由 usb_endp.c 提供真实函数定义。此处补入函数原型，供
 * usb_istr.c 的 pEpInt_IN / pEpInt_OUT 数组在编译期识别这两个符号。
 * 其余 12 个 EP 回调仍在 usb_conf.h 中宏定义为 NOP_Process，无需声明。 */
void EP1_IN_Callback(void);
void EP2_OUT_Callback(void);
```

`src/usb_istr.c`（include 区）：
```c
#include "usb_lib.h"
#include "usb_pwr.h"
#include "usb_istr.h"   /* TR2-A3: EP1_IN_Callback / EP2_OUT_Callback 函数原型 */
```

**注意**：`usb_istr.c` 在 TR1-A4 时未包含 `usb_istr.h`（定义 `USB_Istr` 不需要前置声明），A3 必须补上 include 才能让原型生效。

---

## 6. 实现要点与风险

### 6.1 A3 的可观测变化

**A2 → A3 的 USBTreeView 变化**：

| 项 | A2 | A3 | 变化 |
|---|---|---|---|
| Problem Code | 10 | 10（可能不变） | CBW 被接收但全返回 FAILED |
| Used Endpoints | 1 | 可能仍 1 | 取决于 USBSTOR 是否打开 EP1/EP2 |
| 设备管理器 | USB 大容量存储设备 | 可能无变化 | — |

**分析**：A3 后 USBSTOR 发 CBW（通过 EP2 OUT），设备接收后 `CBW_Decode()` 走 `default` 分支返回 `CSW_CMD_FAILED`。USBSTOR 收到 FAILED 后可能重试或标记错误。Problem Code 可能从 10 变化，也可能不变——取决于 Windows USBSTOR 驱动对 FAILED 响应的处理。

### 6.2 `CBW_Decode()` 中 `SCSI_READ10`/`SCSI_WRITE10` 宏的引用

`CBW_Decode()` 中有：
```c
if ((CBW.CB[0] == SCSI_READ10) || (CBW.CB[0] == SCSI_WRITE10)) {
    SCSI_LBA = ...;
    SCSI_BlkLen = ...;
}
```

`SCSI_READ10`/`SCSI_WRITE10` 宏定义在 `usb_scsi.h`（B1）。A3 阶段 `usb_scsi.h` 不存在。

**处理**：在 `usb_bot.h` 中补入这两个宏定义：
```c
#define SCSI_READ10   0x28
#define SCSI_WRITE10  0x2A
```

B1 创建 `usb_scsi.h` 时会重复定义——需要用 `#ifndef` 守卫，或 B1 时从 `usb_bot.h` 删除。

**更简单的方案**：直接用数值 `0x28`/`0x2A` 替代宏，加注释。B1 时改回宏。

### 6.3 `MAX_LUN` 宏的引用

`CBW_Decode()` 中 `CBW.bLUN > Max_Lun`。参考例程用 `Max_Lun` 变量（`usb_prop.c` 中定义）。本项目 A3 阶段直接用 `MAX_LUN` 宏（`mass_mal.h` 中定义为 0）：

```c
if ((CBW.bLUN > MAX_LUN) || ...)
```

需要 `#include "mass_mal.h"`。但 A3 的 `usb_bot.c` 也可以不 include `mass_mal.h`，直接硬编码 `CBW.bLUN > 0`。B1 时改回 `MAX_LUN` 宏。

**选择**：include `mass_mal.h`，用 `MAX_LUN` 宏，保持代码清晰。

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
| 2 | Problem Code | 可能 10 或变化 | 观察记录 |

### 7.3 链接验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 查看 `.map` 文件 | `EP1_IN_Callback` 指向 `usb_endp.o` 而非 `NOP_Process` | 待验证 |
| 2 | 查看 `.map` 文件 | `EP2_OUT_Callback` 指向 `usb_endp.o` | 待验证 |

---

## 8. 常见问题排查

### 8.1 编译报错：EP1_IN_Callback 重复定义

**原因**：`usb_conf.h` 的宏未注释，同时 `usb_endp.c` 定义了同名函数。

**修复**：确认 `usb_conf.h` 中 `EP1_IN_Callback` 和 `EP2_OUT_Callback` 宏已注释掉。

### 8.2 链接报错：undefined symbol EP1_IN_Callback

**原因**：`usb_conf.h` 宏已注释，但 `usb_endp.c` 未加入工程。

**修复**：确认 `project.uvprojx` 的 src Group 已加入 `usb_endp.c`。

### 8.3 链接报错：undefined symbol SCSI_READ10

**原因**：`CBW_Decode()` 引用 `SCSI_READ10` 宏，但 `usb_scsi.h` 不存在。

**修复**：在 `usb_bot.h` 中补入 `#define SCSI_READ10 0x28`（或用数值替代）。

### 8.4 编译报错：undefined identifier EP1_IN_Callback / EP2_OUT_Callback

**原因**：`usb_conf.h` 宏已注释，但 `usb_istr.c` 的 `pEpInt_IN/OUT` 数组仍引用这两个符号，且无函数原型声明（TR1-A4 的 `usb_istr.h` 精简版未声明 EP 回调）。符号可见性链路 `usb_istr.c → usb_lib.h → usb_type.h → usb_conf.h` 中断。

**修复**：
1. 在 `inc/usb_istr.h` 中补入 `void EP1_IN_Callback(void);` 和 `void EP2_OUT_Callback(void);`
2. 在 `src/usb_istr.c` 中加入 `#include "usb_istr.h"`

详见 §5.5。

---

## 9. 与下一步的衔接

TR2-A3 完成后，BOT 状态机骨架就位，EP1/EP2 回调已接管。下一步：

- **TR2-A4**：`memory.c/h` 缓冲调度骨架。A4 的 `Read_Memory`/`Write_Memory` 被 `#if 0` 包裹（A3 已处理）。
- **TR2-B1**：`usb_scsi.c/h` SCSI 查询命令实现。B1 取消 `usb_bot.c` 中 `CBW_Decode()` 的 `#if 0` 包裹，加回 `#include "usb_scsi.h"`。Problem Code 10 消失。

> **A3 是 TR2 的枢纽**：端点回调从 NOP 切换到真实 BOT 函数，CBW/CSW 收发机制建立。后续 B1 只需取消 `#if 0` 包裹并实现 SCSI 命令，BOT 状态机框架不变。

---

*文档结束。*
