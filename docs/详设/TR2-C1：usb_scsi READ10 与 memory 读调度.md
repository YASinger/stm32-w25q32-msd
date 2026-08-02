# TR2-C1 详细设计：SCSI READ10 与 memory 读调度

## 修改记录

| 版本 | 日期 | 修改内容 | 修改人 |
|---|---|---|---|
| V1.0 | 2026-08-02 | 初始版本 | Copilot |

---

## 文档信息

| 项目 | 内容 |
|---|---|
| 需求编号 | TR2-C1 |
| 需求描述 | `memory.c` Read_Memory 实现（恢复 MAL_Read）+ `usb_scsi.c` SCSI READ10 实现（SCSI_Read10_Cmd + SCSI_Address_Management）+ `usb_bot.c` READ10 case 与 `Mass_Storage_In` 的 `BOT_DATA_IN` 分支恢复 |
| 验收标准 | **可读取磁盘内容**（格式化前的 0xFF 填充或格式化后的 MBR/FAT 数据） |
| 所属阶段 | TR2-C — 读写命令走通（横向补全） |
| 前置文档 | `项目框架设计.md`、`TR2框架设计.md`（§3.3）、`docs/详设/TR2-B1：usb_scsi SCSI 查询命令.md` |
| 前置代码 | TR2-B1 已完成（版本 1.2.1）：9 个查询命令走通，Problem Code 10 消失，PC 显示 8KB 磁盘 |
| 完成态参照 | `tmp\Mass_Storage\src\usb_scsi.c`（SCSI_Read10_Cmd / SCSI_Address_Management）、`tmp\Mass_Storage\src\memory.c`（Read_Memory）、`tmp\Mass_Storage\src\usb_bot.c`（BOT_DATA_IN 分支） |

---

## 1. 需求分解

TR2-C1 是 TR2 阶段**第一条数据命令**。B1 之后设备管理器显示 8KB 磁盘（disk.sys 挂载、卷 D: 创建），但**无法读取磁盘内容**——资源管理器双击盘符提示"需要格式化"，因为 Windows 读 MBR 时 READ10 没有响应（`CBW_Decode` 的 READ10 case 仍在 `#if 0`，走 default 返回 `CSW_CMD_FAILED`）。

C1 打通 READ10 读路径后，PC 能读到磁盘内容（0xFF），这是格式化的前提。

**C1 的四件事**：
1. `usb_scsi.c/h`：实现 `SCSI_Read10_Cmd`（READ10 命令入口）+ `SCSI_Address_Management`（LBA/长度校验），头文件补 `bool` 类型依赖
2. `usb_bot.c`：恢复 `CBW_Decode()` 的 READ10 case 与 `Mass_Storage_In()` 的 `BOT_DATA_IN` 分支（`#if 0` → 恢复编译）
3. `memory.c`：恢复 `Read_Memory()` 中的 `MAL_Read` 调用（读 512B → 64B×8 分包）
4. `mass_mal.c`：`MAL_Init` 磁盘初始内容从全 0 改为全 **0xFF**（对齐框架 §3.3 的 0xFF 验收，模拟 Flash 擦除态）

**与完成态的差异**：完成态还有 WRITE10（C2）、VERIFY10/FORMAT_UNIT（C3）。C1 边界：**只打通 READ10 读路径**，写入路径（`Mass_Storage_Out` 的 `BOT_DATA_OUT` 分支、`Write_Memory` 的 `MAL_Write`）保持 `#if 0`。

本任务修改 5 个文件，无新建文件、无工程文件改动：

| 操作 | 文件 | 内容 |
|---|---|---|
| 修改 | `inc/usb_scsi.h` | 补 `#include "usb_type.h"`（bool 类型）；追加 `SCSI_Read10_Cmd` / `SCSI_Address_Management` 声明 |
| 修改 | `src/usb_scsi.c` | 实现 `SCSI_Read10_Cmd` + `SCSI_Address_Management`；追加 `extern uint8_t Bot_State;` 与 `#include "memory.h"` |
| 修改 | `src/usb_bot.c` | 恢复 `CBW_Decode` 的 READ10 case；恢复 `Mass_Storage_In` 的 `BOT_DATA_IN` 分支 |
| 修改 | `src/memory.c` | 恢复 `Read_Memory` 的 `MAL_Read` 调用 |
| 修改 | `src/mass_mal.c` | `MAL_Init` 的 `memset` 填充值 0 → 0xFF |

---

## 2. 关键技术决策

### 2.1 C1 的命令边界：只恢复 READ10

`CBW_Decode()` 的 switch 中 4 个数据命令 case，C1 只恢复 1 个：

| 命令 | B1 | C1 | C2/C3 |
|---|---|---|---|
| **READ10** | `#if 0` | ✅ **恢复**（C1） | — |
| WRITE10 | `#if 0` | 保持 | C2 恢复 |
| VERIFY10 | `#if 0` | 保持 | C3 恢复 |
| FORMAT_UNIT | `#if 0` | 保持 | C3 恢复 |

两个端点回调的数据分支：
- `Mass_Storage_In()` 的 `BOT_DATA_IN` 分支：✅ **C1 恢复**（依赖 SCSI_Read10_Cmd）
- `Mass_Storage_Out()` 的 `BOT_DATA_OUT` 分支：保持 `#if 0`（依赖 SCSI_Write10_Cmd，C2）

**依据**：READ10 是读路径的入口与续传驱动，二者必须一起恢复；写路径恢复会引用不存在的 `SCSI_Write10_Cmd` 导致链接错误。

### 2.2 READ10 数据路径：MAL 读 512B → 端点 64B×8 分包

USB 端点包 64B，逻辑块 512B。`Read_Memory` 拆包，状态机逐包驱动：

```
首次进入 (Bot_State == BOT_IDLE):
  SCSI_Read10_Cmd → SCSI_Address_Management 校验通过
                  → Bot_State = BOT_DATA_IN
                  → Read_Memory (TXFR_IDLE: 算 Offset/Length, 置 TXFR_ONGOING)
                  → MAL_Read(lun, Offset, Data_Buffer, 512)   ← 一次读 512B
                  → USB_SIL_Write(EP1_IN, Data_Buffer, 64)    ← 发第 1 包
  EP1 IN 中断 → Mass_Storage_In (BOT_DATA_IN) → SCSI_Read10_Cmd (BOT_DATA_IN 分支)
                → Read_Memory 续传: 发第 2~8 包 (Data_Buffer + Block_offset)
  Length == 0 → Bot_State = BOT_DATA_IN_LAST
  下一次 IN 中断 → Mass_Storage_In (BOT_DATA_IN_LAST) → Set_CSW(CSW_CMD_PASSED)
```

关键状态变量（memory.c 已有，A4 骨架保留）：
- `Block_Read_count`：当前 512B 块内剩余未发包数（首发后 = 512−64 = 448）
- `Block_offset`：当前块内已发字节偏移
- `TransferState`（TXFR_IDLE/TXFR_ONGOING）：区分多包续传的首次进入
- `static Offset, Length`：字节级偏移与剩余长度（多次进入间保持）

每发一包：`SetEPTxCount(ENDP1, 64)` → `SetEPTxStatus(ENDP1, EP_TX_VALID)` → `CSW.dDataResidue -= 64`。**顺序保持参考例程**（先写数据再使能端点），否则丢包。

### 2.3 `bool` 类型来源：usb_type.h

`SCSI_Address_Management` 返回 `bool`，本项目定义在 USB 库的 `Library/STM32_USB-FS-Device_Driver/inc/usb_type.h`：

```c
typedef enum { FALSE = 0, TRUE = !FALSE } bool;
```

B1 的 `usb_scsi.h` 只包含 `stm32f10x.h` + `scsi_data.h`，都不提供 `bool`。C1 头文件要声明 `bool` 返回函数，因此**在 `usb_scsi.h` 中补 `#include "usb_type.h"`**（参考例程 usb_scsi.h 正是 `hw_config.h` + `usb_type.h`；本工程去掉 hw_config.h 依赖，直接含无额外依赖的 usb_type.h，自包含成立）。

> 备选：把 `usb_scsi.c` 的 include 顺序改为 `usb_lib.h` 在前。但 `usb_scsi.c` 自身 `usb_scsi.h` 在最前，靠顺序不可靠；**选择在头文件补包含**，与参考例程一致且自包含。

### 2.4 地址校验：SCSI_Address_Management

READ10/WRITE10 共用校验函数（C2 直接复用，故保留 `Cmd == SCSI_WRITE10` 分支）：

| 校验项 | 条件 | 结果 |
|---|---|---|
| LBA 越界 | `(LBA + BlockNbr) > Mass_Block_Count[lun]` | Bot_Abort + Sense `ADDRESS_OUT_OF_RANGE` + `CSW_CMD_FAILED`(DISABLE) → FALSE |
| 长度不匹配 | `CBW.dDataLength != BlockNbr * Mass_Block_Size[lun]` | Bot_Abort + Sense `INVALID_FIELED_IN_COMMAND` + `CSW_CMD_FAILED`(DISABLE) → FALSE |
| 通过 | — | TRUE |

C1 中 `Cmd` 恒为 `SCSI_READ10`，WRITE10 分支不执行，但保留保证与完成态一致（C2 零改动复用）。

### 2.5 SCSI_Read10_Cmd 的双状态处理

- `Bot_State == BOT_IDLE`（CBW 刚解码）：校验 → 检查 `CBW.bmFlags & 0x80`（IN 方向）→ 置 `BOT_DATA_IN` → 调 `Read_Memory` 首发。方向错则 `Bot_Abort(BOTH_DIR)` + Sense + FAILED
- `Bot_State == BOT_DATA_IN`（EP1 IN 中断续传）：直接调 `Read_Memory` 发下一包

### 2.6 SRAM 初始内容改为 0xFF

`mass_mal.c` 的 `MAL_Init`（A1）当前 `memset(sram_disk, 0, ...)`（全 0），但 TR2 框架 §3.3/§8.3 的 C1 验收明确写"格式化前的 **0xFF** 填充"。

**决策**：C1 将填充值改为 `0xFF`，理由：
1. 对齐框架验收——C1 实测"读到 0xFF"是文档承诺的可观测结果
2. 模拟 Flash 擦除态（全 1），与 TR3 换 W25Q32 Flash 后的行为一致，避免届时再改
3. 改动仅 1 行，风险低

> 注意：这是对 A1 文件的改动。B1 详设 §4.3 曾声明 A1 文件不修改，但前提是"行为无变化"；0xFF 填充是 C1 数据路径的必要可观测条件，属 C1 合理范围，CHANGELOG 中说明即可。

### 2.7 工程文件无需改动

`usb_scsi.c/h` 已注册进 `project.uvprojx`（B1 完成），C1 无新建文件，工程文件保持不动。

---

## 3. 涉及组件

```
USBSTOR 发 READ10 CBW (dDataLength = BlockNbr×512, bmFlags IN)
    │
    └── EP2 OUT 中断 → Mass_Storage_Out (BOT_IDLE) → CBW_Decode (usb_bot.c)
        │  case SCSI_READ10  ← C1 恢复 (#if 0 → 编译)
        └── SCSI_Read10_Cmd (usb_scsi.c, C1 新增)
            │  ├─ SCSI_Address_Management (LBA/长度校验, C1 新增)
            │  └─ Bot_State = BOT_DATA_IN
            └── Read_Memory (memory.c, C1 恢复 MAL_Read)
                │  ├─ MAL_Read: SRAM 512B → Data_Buffer (mass_mal.c)
                │  └─ USB_SIL_Write: 64B×8 → EP1 IN
                └── EP1 IN 中断 → Mass_Storage_In (BOT_DATA_IN, C1 恢复)
                    └── SCSI_Read10_Cmd (BOT_DATA_IN 分支) → Read_Memory 续传
                        └── Length==0 → BOT_DATA_IN_LAST → Set_CSW(PASSED)
```

数据流：READ10 CBW → `CBW_Decode` 分发 → `SCSI_Read10_Cmd` 校验 → `Read_Memory` 调 `MAL_Read` 读 512B → 64B×8 分包经 EP1 IN 发送 → IN 中断续传 → 发完置 `BOT_DATA_IN_LAST` → `Set_CSW(CSW_CMD_PASSED)`。四层（bot/scsi/buffer/mal）首次在**数据路径**上联动。

---

## 4. 文件清单

### 4.1 新建文件

无。

### 4.2 修改文件

| 文件 | 修改内容 |
|---|---|
| `inc/usb_scsi.h` | include 区补 `#include "usb_type.h"`；追加 `SCSI_Read10_Cmd` / `SCSI_Address_Management` 声明 |
| `src/usb_scsi.c` | 追加 `#include "memory.h"`、`extern uint8_t Bot_State;`；新增 `SCSI_Read10_Cmd` / `SCSI_Address_Management` 实现 |
| `src/usb_bot.c` | `CBW_Decode` 的 READ10 case 移出 `#if 0`（WRITE10/VERIFY10/FORMAT_UNIT 保留）；`Mass_Storage_In` 恢复 `BOT_DATA_IN` 分支 |
| `src/memory.c` | `Read_Memory` 恢复 `MAL_Read` 调用（去掉 `#if 0`） |
| `src/mass_mal.c` | `MAL_Init`：`memset(sram_disk, 0, ...)` → `memset(sram_disk, 0xFF, ...)` |

### 4.3 不修改的文件

`memory.h`（Read_Memory 声明已存在）、`usb_bot.h`（BOT_* 状态宏、SCSI_READ10 占位已存在）、`scsi_data.c/h`、`mass_mal.h`、`usb_endp.c`、`usb_conf.h`、`project.uvprojx` 在 C1 均不变。

---

## 5. 接口设计

### 5.1 `inc/usb_scsi.h`

**(a) include 区补 `usb_type.h`（bool 类型来源）**：

```c
/* Includes ------------------------------------------------------------------*/
#include "stm32f10x.h"
#include "usb_type.h"    /* C1: bool 类型 (SCSI_Address_Management 返回类型) */
#include "scsi_data.h"   /* 数据长度宏 + 静态数据 extern (A2 定义) */
```

**(b) 函数声明追加**（替换原"待 C1~C3 再声明"注释）：

```c
/* C1: READ10 数据命令 */
void SCSI_Read10_Cmd(uint8_t lun, uint32_t LBA, uint32_t BlockNbr);
bool SCSI_Address_Management(uint8_t lun, uint8_t Cmd, uint32_t LBA, uint32_t BlockNbr);

/* C2~C3 再声明: SCSI_Write10_Cmd / SCSI_Verify10_Cmd / SCSI_Format_Cmd */
```

> `SCSI_READ10` 宏已在 B1 定义（`#ifndef` 守卫，0x28），无需重复定义。

### 5.2 `src/usb_scsi.c`

三处改动：

**(a) include 区追加 `memory.h`**（`SCSI_Read10_Cmd` 调用 `Read_Memory`，声明来自 memory.h，B1 未包含）：

```c
/* Includes ------------------------------------------------------------------*/
#include "usb_scsi.h"
#include "scsi_data.h"
#include "mass_mal.h"      /* MAL_GetStatus / Mass_Block_* */
#include "usb_bot.h"       /* CBW/CSW / Bot_Abort / Set_CSW / Transfer_Data_Request */
#include "memory.h"        /* C1: Read_Memory (READ10 数据发送) */
#include "usb_lib.h"       /* USB 库 (一致性) */
```

**(b) extern 区追加 `Bot_State`**（B1 已声明 CBW/CSW/Mass_Block_Size/Mass_Block_Count）：

```c
/* External variables --------------------------------------------------------*/
extern Bulk_Only_CBW CBW;
extern Bulk_Only_CSW CSW;
extern uint32_t Mass_Block_Size[2];
extern uint32_t Mass_Block_Count[2];
extern uint8_t Bot_State;    /* C1: SCSI_Read10_Cmd 判断 BOT_IDLE/BOT_DATA_IN */
```

**(c) 追加两个函数实现**（参考例程原样，注释按本项目风格）：

```c
/*******************************************************************************
* Function Name  : SCSI_Read10_Cmd
* Description    : SCSI Read10 Command (0x28)。
*                  BOT_IDLE 进入: 地址校验 → 置 BOT_DATA_IN → Read_Memory 首发；
*                  BOT_DATA_IN 进入: EP1 IN 中断续传, 再调 Read_Memory 发下一包。
* Input          : lun - 逻辑单元号; LBA - 起始逻辑块; BlockNbr - 块数
* Output         : None.
* Return         : None.
*******************************************************************************/
void SCSI_Read10_Cmd(uint8_t lun, uint32_t LBA, uint32_t BlockNbr)
{
  if (Bot_State == BOT_IDLE)
  {
    if (!(SCSI_Address_Management(CBW.bLUN, SCSI_READ10, LBA, BlockNbr)))
    {
      return;   /* 地址/长度非法, SCSI_Address_Management 已做错误处理 */
    }

    if ((CBW.bmFlags & 0x80) != 0)   /* IN 方向 */
    {
      Bot_State = BOT_DATA_IN;
      Read_Memory(lun, LBA, BlockNbr);
    }
    else
    {
      Bot_Abort(BOTH_DIR);
      Set_Scsi_Sense_Data(CBW.bLUN, ILLEGAL_REQUEST, INVALID_FIELED_IN_COMMAND);
      Set_CSW(CSW_CMD_FAILED, SEND_CSW_ENABLE);
    }
    return;
  }
  else if (Bot_State == BOT_DATA_IN)
  {
    Read_Memory(lun, LBA, BlockNbr);
  }
}

/*******************************************************************************
* Function Name  : SCSI_Address_Management
* Description    : READ10/WRITE10 共用地址校验 (C2 复用, 保留 WRITE10 分支)。
*                  校验 LBA 越界与 CBW 声明长度不匹配。
* Input          : lun - 逻辑单元号; Cmd - SCSI_READ10/SCSI_WRITE10;
*                  LBA - 起始逻辑块; BlockNbr - 块数
* Output         : None.
* Return         : bool - TRUE 校验通过 / FALSE 失败 (已做错误处理)
*******************************************************************************/
bool SCSI_Address_Management(uint8_t lun, uint8_t Cmd, uint32_t LBA, uint32_t BlockNbr)
{
  if ((LBA + BlockNbr) > Mass_Block_Count[lun])
  {
    if (Cmd == SCSI_WRITE10)
    {
      Bot_Abort(BOTH_DIR);
    }
    Bot_Abort(DIR_IN);
    Set_Scsi_Sense_Data(lun, ILLEGAL_REQUEST, ADDRESS_OUT_OF_RANGE);
    Set_CSW(CSW_CMD_FAILED, SEND_CSW_DISABLE);
    return (FALSE);
  }

  if (CBW.dDataLength != BlockNbr * Mass_Block_Size[lun])
  {
    if (Cmd == SCSI_WRITE10)
    {
      Bot_Abort(BOTH_DIR);
    }
    else
    {
      Bot_Abort(DIR_IN);
    }
    Set_Scsi_Sense_Data(CBW.bLUN, ILLEGAL_REQUEST, INVALID_FIELED_IN_COMMAND);
    Set_CSW(CSW_CMD_FAILED, SEND_CSW_DISABLE);
    return (FALSE);
  }
  return (TRUE);
}
```

> **注意**：`SCSI_Read10_Cmd` 必须在 `usb_bot.c` 恢复 READ10 case **之前**实现，否则 `undefined symbol`（链接失败）。两个文件同一次提交。

### 5.3 `src/usb_bot.c`

**(a) `CBW_Decode()` — READ10 case 移出 `#if 0`**：

```c
        case SCSI_READ10:
          SCSI_Read10_Cmd(CBW.bLUN, SCSI_LBA, SCSI_BlkLen);
          break;
#if 0  /* === 数据命令 (C2/C3 恢复) === */
        case SCSI_WRITE10:
          SCSI_Write10_Cmd(CBW.bLUN, SCSI_LBA, SCSI_BlkLen);
          break;
        case SCSI_VERIFY10:
          SCSI_Verify10_Cmd(CBW.bLUN);
          break;
        case SCSI_FORMAT_UNIT:
          SCSI_Format_Cmd(CBW.bLUN);
          break;
#endif
```

**(b) `Mass_Storage_In()` — 恢复 `BOT_DATA_IN` 分支**（取消 `#if 0`，注释同步更新）：

```c
    case BOT_DATA_IN:
      switch (CBW.CB[0])
      {
        case SCSI_READ10:
          SCSI_Read10_Cmd(CBW.bLUN, SCSI_LBA, SCSI_BlkLen);
          break;
      }
      break;
```

### 5.4 `src/memory.c`

`Read_Memory` 恢复 `MAL_Read` 调用（去掉 `#if 0` 包裹），其余逻辑 A4 已就位无需改动：

```c
  if (TransferState == TXFR_ONGOING )
  {
    if (!Block_Read_count)
    {
      MAL_Read(lun ,
               Offset ,
               Data_Buffer,
               Mass_Block_Size[lun]);

      USB_SIL_Write(EP1_IN, (uint8_t *)Data_Buffer, BULK_MAX_PACKET_SIZE);
      ...
```

### 5.5 `src/mass_mal.c`

`MAL_Init` 填充值 0 → 0xFF（§2.6）：

```c
uint16_t MAL_Init(uint8_t lun)
{
    if (lun != 0) return MAL_FAIL;

    /* SRAM 无需硬件初始化; 填充 0xFF 模拟 Flash 擦除态 (C1) */
    memset(sram_disk, 0xFF, SRAM_DISK_SIZE);
    return MAL_OK;
}
```

---

## 6. 实现要点与风险

### 6.1 可观测变化（B1 → C1）

| 项 | B1 | C1 |
|---|---|---|
| 读取磁盘内容 | 无法读取（READ10 无响应） | **可读取（全 0xFF）** |
| 资源管理器双击盘符 | 提示"需要格式化"或无法访问 | 仍提示未格式化（MBR 不存在），但行为不同：Windows 已成功发出 READ10 读 MBR，读到 0xFF 判定"未格式化" |
| 格式化 | 不可用 | 仍不可用（WRITE10 未实现，C2） |

### 6.2 中断时序风险（核心）

EP1 IN 中断是 READ10 多包续传的唯一驱动力。`Read_Memory` 每包执行顺序**必须**是：`USB_SIL_Write`（填数据）→ `SetEPTxCount`（写长度）→ `SetEPTxStatus(EP_TX_VALID)`（使能发送）。若顺序颠倒或漏掉 `SetEPTxStatus`，端点不会真正发送，状态机停在 `BOT_DATA_IN`，主机读取超时。

### 6.3 LBA 越界风险

Windows 格式化探测阶段可能发出越界 READ10（如 LBA=16 或长度超过 8KB）。`SCSI_Address_Management` 返回 FALSE 并 Stall + CSW_CMD_FAILED 是正确响应，驱动会据此停止探测。**不要**在越界时返回数据，否则驱动误判介质损坏。

### 6.4 链接风险

- 恢复 READ10 case 必须同时提供 `SCSI_Read10_Cmd` 定义，否则 `undefined symbol`（§5.2 提示）
- `SCSI_Read10_Cmd` 引用 `Read_Memory`（memory.h）、`Bot_State`（usb_bot.c 全局）——确认 extern 齐全
- `bool` 未定义：`usb_scsi.h` 必须包含 `usb_type.h`（§2.3）

### 6.5 数据错乱风险

- `MAL_Read` 的 `Memory_Offset` 是**字节偏移**（`LBA × 512`），不是块号——`Read_Memory` 在 TXFR_IDLE 分支已换算，勿改动
- `Data_Buffer` 为 `uint32_t` 数组但按字节访问（`(uint8_t *)Data_Buffer`），长度 512B 恰为 128 个 uint32_t，无对齐问题
- `CSW.dDataResidue` 每包减 64，发完归零才能 Passed；若中途异常，残留值会导致驱动报错

---

## 7. 验证流程

### 7.1 编译验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | Keil Build (F7) | 0 Error(s), 0 Warning(s) | 待验证 |

### 7.2 USBTreeView / 设备管理器验证（回归）

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 烧录，插 USB | **Problem Code 保持消失** | C1 回归 ✅ |
| 2 | 磁盘容量 | 约 8KB（16 块 × 512B）不变 | C1 回归 ✅ |

### 7.3 磁盘内容读取（核心验收，框架 §8.3 第 1 条）

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | WinHex（或 HxD）→ Tools → Open Disk → 选 STM32 SRAM Disk | 读取 8KB 磁盘内容 | TR2-C1 ✅ |
| 2 | 查看扇区数据 | **全 0xFF**（未格式化；MAL_Init 已改） | TR2-C1 ✅ |
| 3 | 资源管理器双击 D: | 提示"需要格式化"（Windows 已成功发出 READ10，读到 0xFF 判定无 MBR） | 间接佐证 ✅ |
| 4 | 尝试格式化 | 不可用（预期，WRITE10 未实现） | C1 边界确认 |

### 7.4 链接验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 查看 `.map` 文件 | `SCSI_Read10_Cmd` / `SCSI_Address_Management` 来自 `usb_scsi.o`，`Read_Memory` 来自 `memory.o` | 待验证 |

---

## 8. 常见问题排查

### 8.1 读取超时 / 资源管理器报 I/O 错误

**原因**：EP1 IN 中断未续传，状态机停在 `BOT_DATA_IN`（`SetEPTxStatus` 顺序错误或 IN 中断未触发 `Mass_Storage_In`）。

**排查**：检查 `Mass_Storage_In` 的 `BOT_DATA_IN` 分支是否恢复；检查 `Read_Memory` 每包的 `USB_SIL_Write → SetEPTxCount → SetEPTxStatus` 顺序（§6.2）。

### 8.2 读到 0x00 而不是 0xFF

**原因**：`MAL_Init` 仍是 `memset(..., 0, ...)`，或烧录后未重新上电（SRAM 内容残留旧固件写入值）。

**排查**：确认 §5.5 改动；重新烧录并拔插 USB。

### 8.3 编译报错：`bool` 未定义

**原因**：`usb_scsi.h` 未包含 `usb_type.h`。

**修复**：确认 §5.1(a) 的 `#include "usb_type.h"` 已加。

### 8.4 链接报错：`undefined symbol SCSI_Read10_Cmd`

**原因**：`usb_bot.c` 恢复了 READ10 case，但 `usb_scsi.c` 未实现/未加入工程。

**修复**：确认两个函数已实现（§5.2c），`usb_scsi.c` 已在 `project.uvprojx`。

### 8.5 数据错乱（读到内容与预期偏移不符）

**原因**：`MAL_Read` 的 `Memory_Offset` 被当作块号；或大端/小端混淆。

**修复**：`Read_Memory` 用 `Memory_Offset * Mass_Block_Size[lun]` 换算字节偏移，保持 A4 原样（§6.5）。

### 8.6 READ10 响应但只回一包就停

**原因**：`Mass_Storage_In` 的 `BOT_DATA_IN` 分支未恢复，IN 中断后走 `BOT_DATA_IN_LAST` 提前发 CSW。

**修复**：确认 §5.3(b) 分支恢复。

---

## 9. 与下一步的衔接

TR2-C1 完成后，PC 能读取磁盘内容（全 0xFF），这是 TR2-C 的第一块补全。后续：

- **TR2-C2**：`usb_scsi.c` 实现 `SCSI_Write10_Cmd`（复用 `SCSI_Address_Management`），`memory.c` 恢复 `Write_Memory` 的 `MAL_Write`，`usb_bot.c` 恢复 WRITE10 case 与 `Mass_Storage_Out` 的 `BOT_DATA_OUT` 分支 → **可格式化为 FAT**，可创建/写入/读取/删除文件
- **TR2-C3**：`VERIFY10`（`BLKVFY` 宏已预定义）/`FORMAT_UNIT` 等命令补全 + 安全弹出验证 → TR2 阶段全部完成

> **C1 是 TR2-C 的分水岭**：数据路径（READ10）从"骨架可编译"进入"真实可读"。C2 的 WRITE10 结构完全对称（OUT 方向 + 组包），`SCSI_Address_Management` 与 `TransferState` 机制已就位，C2 只需按 C1 模式镜像实现。

---

*文档结束。*

