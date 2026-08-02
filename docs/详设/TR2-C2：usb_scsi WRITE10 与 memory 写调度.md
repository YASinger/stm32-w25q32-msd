# TR2-C2 详细设计：SCSI WRITE10 与 memory 写调度

## 修改记录

| 版本 | 日期 | 修改内容 | 修改人 |
|---|---|---|---|
| V1.0 | 2026-08-02 | 初始版本 | Copilot |
| V1.1 | 2026-08-02 | **实测修订**：Windows 无法格式化 ≤16KB 的 FAT 卷（`format` 报"卷对 FAT16/12 来说太大"，最小卷 ≥32KB），SRAM 20KB 无法满足 → 验收改为"写路径验证"，格式化 + 文件读写顺延 TR3；补 `scsi_data.c` Block Length 修复（0x02→0x00） | Copilot |

---

## 文档信息

| 项目 | 内容 |
|---|---|
| 需求编号 | TR2-C2 |
| 需求描述 | `usb_scsi.c` SCSI WRITE10 实现（`SCSI_Write10_Cmd`，复用 `SCSI_Address_Management`）+ `memory.c` `Write_Memory` 恢复 `MAL_Write` + `usb_bot.c` WRITE10 case 与 `Mass_Storage_Out` 的 `BOT_DATA_OUT` 分支恢复 |
| 验收标准 | **写路径验证通过**：WinHex 直接写扇区 → 读回一致（V1.1：Windows 无法格式化 ≤16KB SRAM 卷，格式化 + 文件读写顺延 TR3） |
| 所属阶段 | TR2-C — 读写命令走通（横向补全） |
| 前置文档 | `项目框架设计.md`、`TR2框架设计.md`（§3.3/§6.1/§8.3）、`docs/详设/TR2-C1：usb_scsi READ10 与 memory 读调度.md` |
| 前置代码 | TR2-C1 已完成（版本 1.3.1）：READ10 读路径打通，WinHex 读到全 0xFF；`SCSI_Address_Management` 已实现且保留 WRITE10 分支 |
| 完成态参照 | `tmp\Mass_Storage\src\usb_scsi.c`（SCSI_Write10_Cmd）、`tmp\Mass_Storage\src\memory.c`（Write_Memory）、`tmp\Mass_Storage\src\usb_bot.c`（BOT_DATA_OUT 分支） |

---

## 1. 需求分解

TR2-C2 是 TR2 阶段**第二条数据命令**（写方向），与 C1 的读方向完全对称。C1 之后 PC 能读到磁盘内容（全 0xFF），但**无法写入**——格式化时 Windows 写 MBR/引导扇区/FAT 表全部走 WRITE10，`CBW_Decode` 的 WRITE10 case 仍在 `#if 0`，走 default 返回 `CSW_CMD_FAILED`，于是弹出"Windows 无法完成格式化"（C1 实测已确认此现象，属预期）。

C2 打通 WRITE10 写路径后，PC 能写入磁盘。原预期"可格式化为 FAT → 可创建/写入/读取/删除文件"，但 **V1.1 实测发现**：Windows 无法格式化 ≤16KB 的 FAT 卷（`format D: /FS:FAT` 对 8KB/12KB/16KB 全部报"卷对 FAT16/12 来说太大"，最小 FAT12 卷 ≥32KB），而 SRAM 仅 20KB（磁盘上限 ~17KB）→ **"格式化 + 文件读写"顺延 TR3**。C2 验收聚焦**写路径验证**：写路径本身已完整工作（完全格式化"全盘写 0"成功即为铁证），用 WinHex 直接写扇区并读回确认。

**C2 的三件事**：
1. `usb_scsi.c/h`：实现 `SCSI_Write10_Cmd`（WRITE10 命令入口，复用 C1 的 `SCSI_Address_Management`），头文件补声明
2. `usb_bot.c`：恢复 `CBW_Decode()` 的 WRITE10 case 与 `Mass_Storage_Out()` 的 `BOT_DATA_OUT` 分支（`#if 0` → 恢复编译）
3. `memory.c`：恢复 `Write_Memory()` 中的 `MAL_Write` 调用（EP2 OUT 组包 64B×8 → 512B 写介质）

**与完成态的差异**：完成态还有 VERIFY10/FORMAT_UNIT（C3）。C2 边界：**只打通 WRITE10 写路径**，VERIFY10/FORMAT_UNIT 保持 `#if 0`。

本任务修改 5 个文件，无新建文件、无工程文件改动：

| 操作 | 文件 | 内容 |
|---|---|---|
| 修改 | `inc/usb_scsi.h` | 追加 `SCSI_Write10_Cmd` 声明（替换"C2~C3 再声明"注释） |
| 修改 | `src/usb_scsi.c` | 新增 `SCSI_Write10_Cmd` 实现（参考例程原样）；`SCSI_ReadFormatCapacity_Cmd` 补 `ReadFormatCapacity_Data[8]=0`（V1.1） |
| 修改 | `src/usb_bot.c` | 恢复 `CBW_Decode` 的 WRITE10 case；恢复 `Mass_Storage_Out` 的 `BOT_DATA_OUT` 分支 |
| 修改 | `src/memory.c` | `Write_Memory` 恢复 `MAL_Write` 调用（去掉 `#if 0`） |
| 修改 | `src/scsi_data.c` | `ReadFormatCapacity_Data[8]` 0x02 → 0x00（Block Length 高位修复，V1.1） |

> `mass_mal.c` 的 `MAL_Write` 在 A1 已完整实现（memcpy 覆盖，SRAM 无需擦除），C2 **不修改**。

---

## 2. 关键技术决策

### 2.1 写路径与读路径的对称性

C1 READ10（IN：主机读设备）与 C2 WRITE10（OUT：主机写设备）结构完全对称，C2 按 C1 模式镜像实现：

| 环节 | READ10 (C1) | WRITE10 (C2) |
|---|---|---|
| 命令入口 | `SCSI_Read10_Cmd` | `SCSI_Write10_Cmd` |
| 地址校验 | `SCSI_Address_Management`（复用） | 同左（WRITE10 分支此时生效） |
| 方向判断 | `(bmFlags & 0x80) != 0`（IN） | `(bmFlags & 0x80) == 0`（OUT） |
| 状态置位 | `BOT_DATA_IN` | `BOT_DATA_OUT` |
| 数据驱动 | EP1 IN 中断续传 | EP2 OUT 中断续传 |
| 调度函数 | `Read_Memory`（拆包 512→64×8） | `Write_Memory`（组包 64×8→512） |
| 介质操作 | `MAL_Read` | `MAL_Write` |
| 结束条件 | `Length==0` → `BOT_DATA_IN_LAST` | `W_Length==0` → `Set_CSW(PASSED)` |

### 2.2 SCSI_Address_Management 零改动复用

C1 实现 `SCSI_Address_Management` 时已保留 `Cmd == SCSI_WRITE10` 分支（越界/长度不匹配时 `Bot_Abort(BOTH_DIR)`）。C2 的 `SCSI_Write10_Cmd` 直接调用，**C1 代码零改动**。这是 C1 详设 §2.4 的承诺兑现。

### 2.3 方向校验与 EP2 重新使能

`SCSI_Write10_Cmd` 的 BOT_IDLE 分支（参考例程原样）：

```c
if ((CBW.bmFlags & 0x80) == 0)      /* OUT 方向 */
{
  Bot_State = BOT_DATA_OUT;
  SetEPRxStatus(ENDP2, EP_RX_VALID);  /* 关键: 重新使能 EP2 接收 WRITE10 数据 */
}
else                                  /* 方向错 */
{
  Bot_Abort(DIR_IN);
  Set_Scsi_Sense_Data(CBW.bLUN, ILLEGAL_REQUEST, INVALID_FIELED_IN_COMMAND);
  Set_CSW(CSW_CMD_FAILED, SEND_CSW_DISABLE);
}
```

**关键点**：`SetEPRxStatus(ENDP2, EP_RX_VALID)` 必须在命令入口显式执行——CBW 接收完成（EP2 OUT 传输）后，必须重新使能 EP2 才能接收紧随其后的 WRITE10 数据。读路径（C1）不需要这一步，写路径必须有。

### 2.4 Write_Memory 组包逻辑（A4 骨架已就位）

A4 的 `Write_Memory` 骨架与参考例程逐行一致，C2 只需恢复 `MAL_Write` 调用：

```
TXFR_IDLE (首次进入):
  W_Offset = LBA × 512;  W_Length = 块数 × 512;  TransferState = TXFR_ONGOING

TXFR_ONGOING (每次 EP2 OUT 中断):
  64B 从 Bulk_Data_Buff 拷入 Data_Buffer[Counter..]  (组包)
  W_Offset += Data_Len;  W_Length -= Data_Len
  若 (W_Length % 512) == 0  (凑满一块):
    Counter = 0
    MAL_Write(lun, W_Offset - 512, Data_Buffer, 512)   ← C2 恢复
  CSW.dDataResidue -= Data_Len
  SetEPRxStatus(ENDP2, EP_RX_VALID)   (使能下一次接收)

W_Length == 0 或 Bot_State == BOT_CSW_Send:
  Counter = 0;  Set_CSW(CSW_CMD_PASSED, SEND_CSW_ENABLE);  TransferState = TXFR_IDLE
```

**注意**：`MAL_Write` 的 `Memory_Offset` 是**字节偏移**（`W_Offset - 512`），不是块号——A4 骨架已按参考例程换算，C2 勿改动。

### 2.5 FAT12 格式化（验收的可观测结果）

TR2 框架 §6.1：8KB 磁盘（16 块 × 512B），Windows 会格式化为 **FAT12**（最小 FAT 文件系统，16 扇区可格式化）。格式化过程 = Windows 发一串 WRITE10（写 MBR/引导扇区/FAT 表/根目录），全部走 C2 写路径。格式化成功、容量 ~8KB 是 C2 核心验收。

### 2.6 工程文件无需改动

`usb_scsi.c/h` 已在工程注册（B1 完成），`memory.c`/`usb_bot.c` 本就在工程中，C2 无新建文件，`project.uvprojx` 保持不动。
### 2.7 V1.1 重大发现：Windows 最小 FAT 卷限制（决策变更）

**实测**：`format D: /FS:FAT` 对 8KB/12KB/16KB 磁盘全部报"卷对 FAT16/12 来说太大"，格式化失败。经查证，Windows 的最小 FAT12 卷约 32KB，而 STM32F103C8 仅 20KB SRAM（磁盘上限 ~17KB）→ **SRAM 方案在物理上无法承载可被 Windows 格式化的 FAT 卷**。

**决策**：
1. TR2-C2 验收改为**写路径验证**（写路径本身已证明工作：完全格式化"全盘写 0"成功）
2. "格式化 + 文件读写"顺延 TR3（W25Q32 Flash 4MB，满足最小卷要求）
3. 磁盘保持 8KB 原设计（写路径与大小无关，8KB 保留 RAM 余量）

**附带修复**：排查中发现 `ReadFormatCapacity_Data[8]` 静态值 0x02 会被解析成 33.5MB 的错误 Block Length，已修为 0x00（正确性修复，与格式化失败无因果关系——8KB/12KB/16KB 在修复前后均报同样错误）。


---

## 3. 涉及组件

```
USBSTOR 发 WRITE10 CBW (dDataLength = BlockNbr×512, bmFlags OUT)
    │
    └── EP2 OUT 中断 → Mass_Storage_Out (BOT_IDLE) → CBW_Decode (usb_bot.c)
        │  case SCSI_WRITE10  ← C2 恢复 (#if 0 → 编译)
        └── SCSI_Write10_Cmd (usb_scsi.c, C2 新增)
            │  ├─ SCSI_Address_Management (LBA/长度校验, C1 已实现, WRITE10 分支生效)
            │  ├─ (bmFlags & 0x80)==0 校验 OUT 方向
            │  └─ Bot_State = BOT_DATA_OUT + SetEPRxStatus(ENDP2, EP_RX_VALID)
            │
    EP2 OUT 中断 → Mass_Storage_Out (BOT_DATA_OUT, C2 恢复)
        │  case BOT_DATA_OUT → SCSI_Write10_Cmd (BOT_DATA_OUT 分支)
        └── Write_Memory (memory.c, C2 恢复 MAL_Write)
            │  ├─ 64B 从 Bulk_Data_Buff 拷入 Data_Buffer (组包)
            │  ├─ 满 512B → MAL_Write 写 SRAM (mass_mal.c, A1 已实现)
            │  └─ W_Length==0 → Set_CSW(PASSED)
```

数据流：WRITE10 CBW → `CBW_Decode` 分发 → `SCSI_Write10_Cmd` 校验 + 置 `BOT_DATA_OUT` + 使能 EP2 → EP2 OUT 中断组包 → 满 512B `MAL_Write` 写介质 → 全写完 `Set_CSW(CSW_CMD_PASSED)`。四层（bot/scsi/buffer/mal）在**写路径**上联动（与 C1 读路径对称）。

---

## 4. 文件清单

### 4.1 新建文件

无。

### 4.2 修改文件

| 文件 | 修改内容 |
|---|---|
| `inc/usb_scsi.h` | 追加 `SCSI_Write10_Cmd` 声明（替换"C2~C3 再声明: SCSI_Write10_Cmd / SCSI_Verify10_Cmd / SCSI_Format_Cmd"注释中已完成的部分） |
| `src/usb_scsi.c` | 新增 `SCSI_Write10_Cmd` 实现（`SCSI_Read10_Cmd` 之后，参考例程原样） |
| `src/usb_bot.c` | `CBW_Decode` 的 WRITE10 case 移出 `#if 0`（VERIFY10/FORMAT_UNIT 保留）；`Mass_Storage_Out` 恢复 `BOT_DATA_OUT` 分支 |
| `src/memory.c` | `Write_Memory` 恢复 `MAL_Write` 调用（去掉 `#if 0` 包裹） |

### 4.3 不修改的文件

`mass_mal.c`（MAL_Write A1 已实现）、`memory.h`（Write_Memory 声明已存在）、`usb_bot.h`（BOT_* 状态宏、SCSI_WRITE10 占位已存在）、`scsi_data.c/h`、`usb_endp.c`、`usb_conf.h`、`project.uvprojx` 在 C2 均不变。

---

## 5. 接口设计

### 5.1 `inc/usb_scsi.h`

函数声明追加（替换原"C2~C3 再声明"注释）：

```c
/* C1: READ10 数据命令 */
void SCSI_Read10_Cmd(uint8_t lun, uint32_t LBA, uint32_t BlockNbr);
bool SCSI_Address_Management(uint8_t lun, uint8_t Cmd, uint32_t LBA, uint32_t BlockNbr);

/* C2: WRITE10 数据命令 */
void SCSI_Write10_Cmd(uint8_t lun, uint32_t LBA, uint32_t BlockNbr);

/* C3 再声明: SCSI_Verify10_Cmd / SCSI_Format_Cmd */
```

> `SCSI_WRITE10` 宏已在 B1 定义（`#ifndef` 守卫，0x2A），无需重复定义。

### 5.2 `src/usb_scsi.c`

新增一个函数实现（放在 `SCSI_Read10_Cmd` 之后、`SCSI_Address_Management` 之前或之后均可，参考例程原样，注释按本项目风格）：

```c
/*******************************************************************************
* Function Name  : SCSI_Write10_Cmd
* Description    : SCSI Write10 Command (0x2A)。
*                  BOT_IDLE 进入: 地址校验 → 置 BOT_DATA_OUT → 使能 EP2 接收数据；
*                  BOT_DATA_OUT 进入: EP2 OUT 中断续传, 调 Write_Memory 组包。
* Input          : lun - 逻辑单元号; LBA - 起始逻辑块; BlockNbr - 块数
* Output         : None.
* Return         : None.
*******************************************************************************/
void SCSI_Write10_Cmd(uint8_t lun, uint32_t LBA, uint32_t BlockNbr)
{
  if (Bot_State == BOT_IDLE)
  {
    if (!(SCSI_Address_Management(CBW.bLUN, SCSI_WRITE10, LBA, BlockNbr)))
    {
      return;   /* 地址/长度非法, SCSI_Address_Management 已做错误处理 */
    }

    if ((CBW.bmFlags & 0x80) == 0)   /* OUT 方向 */
    {
      Bot_State = BOT_DATA_OUT;
      SetEPRxStatus(ENDP2, EP_RX_VALID);  /* 使能 EP2 接收 WRITE10 数据 */
    }
    else
    {
      Bot_Abort(DIR_IN);
      Set_Scsi_Sense_Data(CBW.bLUN, ILLEGAL_REQUEST, INVALID_FIELED_IN_COMMAND);
      Set_CSW(CSW_CMD_FAILED, SEND_CSW_DISABLE);
    }
    return;
  }
  else if (Bot_State == BOT_DATA_OUT)
  {
    Write_Memory(lun, LBA, BlockNbr);
  }
}
```

> 依赖说明：`Write_Memory` 声明来自 memory.h（已 include）；`Bot_State` extern 已声明（C1 补的）；`SetEPRxStatus`/`EP_RX_VALID` 来自 usb_lib.h（已 include）；`SCSI_WRITE10` 宏已定义。**无新增依赖**。

### 5.3 `src/usb_bot.c`

**(a) `CBW_Decode()` — WRITE10 case 移出 `#if 0`**：

```c
#if 0  /* === 数据命令 (C3 恢复) === */
        case SCSI_VERIFY10:
          SCSI_Verify10_Cmd(CBW.bLUN);
          break;
        case SCSI_FORMAT_UNIT:
          SCSI_Format_Cmd(CBW.bLUN);
          break;
#endif
```

即把 WRITE10 case 从 `#if 0` 块中移到块外（紧邻 READ10 case 之后）：

```c
        /* === C1: READ10 数据命令 (恢复) === */
        case SCSI_READ10:
          SCSI_Read10_Cmd(CBW.bLUN, SCSI_LBA, SCSI_BlkLen);
          break;
        /* === C2: WRITE10 数据命令 (恢复) === */
        case SCSI_WRITE10:
          SCSI_Write10_Cmd(CBW.bLUN, SCSI_LBA, SCSI_BlkLen);
          break;
#if 0  /* === 数据命令 (C3 恢复) === */
        case SCSI_VERIFY10:
          ...
        case SCSI_FORMAT_UNIT:
          ...
#endif
```

**(b) `Mass_Storage_Out()` — 恢复 `BOT_DATA_OUT` 分支**（取消 `#if 0`，注释同步更新）：

```c
    case BOT_DATA_OUT:
      if (CBW.CB[0] == SCSI_WRITE10)
      {
        SCSI_Write10_Cmd(CBW.bLUN, SCSI_LBA, SCSI_BlkLen);
        break;
      }
      Bot_Abort(DIR_OUT);
      Set_CSW(CSW_PHASE_ERROR, SEND_CSW_DISABLE);
      break;
```

### 5.4 `src/memory.c`

`Write_Memory` 恢复 `MAL_Write` 调用（去掉 `#if 0` 包裹），其余逻辑 A4 已就位无需改动：

```c
    if (!(W_Length % Mass_Block_Size[lun]))
    {
      Counter = 0;
      MAL_Write(lun ,
                W_Offset - Mass_Block_Size[lun],
                Data_Buffer,
                Mass_Block_Size[lun]);
    }
```

> `MAL_Write` 声明来自 mass_mal.h（已 include），`Mass_Block_Size` extern 已声明。

---

## 6. 实现要点与风险

### 6.1 可观测变化（C1 → C2）

| 项 | C1 | C2 |
|---|---|---|
| 磁盘内容 | 全 0xFF（不可写） | **可写**：WinHex 写扇区 → 读回一致 |
| 写路径 | 未实现 | **WRITE10 → Write_Memory → MAL_Write 全链路工作**（完全格式化"全盘写 0"验证） |
| 格式化 | 不可用（"Windows 无法完成格式化"） | 仍不可用（"卷对 FAT16/12 来说太大"）——**V1.1 已知限制，顺延 TR3** |
| 文件操作 | 不可用 | 顺延 TR3（需格式化后才有文件系统） |

### 6.2 EP2 使能时序风险（核心）

WRITE10 数据阶段靠 EP2 OUT 中断驱动。`SetEPRxStatus(ENDP2, EP_RX_VALID)` 有两处必须存在：
1. `SCSI_Write10_Cmd` 的 BOT_IDLE 分支（命令入口，使能第一次数据接收）
2. `Write_Memory` 的 TXFR_ONGOING 分支（每包接收完成后使能下一次）

若漏掉任一处，主机写入会卡住、状态机停在 `BOT_DATA_OUT`，格式化/写文件超时。

### 6.3 方向校验风险

WRITE10 的 CBW `bmFlags` bit7 必须为 0（OUT）。Windows 格式化/写文件时方向恒为 OUT。若收到 IN 方向（异常/攻击），`Bot_Abort(DIR_IN)` + Sense + FAILED 是正确响应。**不要**在方向错时进入 `BOT_DATA_OUT`。

### 6.4 链接风险

- 恢复 WRITE10 case 必须同时提供 `SCSI_Write10_Cmd` 定义，否则 `undefined symbol`（§5.2 提示）
- `SCSI_Write10_Cmd` 引用 `Write_Memory`（memory.h）、`Bot_State`（extern 已声明）——确认依赖齐全

### 6.5 数据错乱风险

- `MAL_Write` 的 `Memory_Offset` 是**字节偏移**（`W_Offset - Mass_Block_Size[lun]`），不是块号——A4 已换算，勿改动
- `Data_Buffer` 为 `uint32_t` 数组但按字节访问（`(uint8_t *)Data_Buffer`），512B 恰为 128 个 uint32_t，无对齐问题
- `CSW.dDataResidue` 每包减 `Data_Len`，归零才能 Passed；若中途异常，残留值会导致驱动报错
- 组包用 `Counter`（已收字节计数）与 `Idx`（Bulk_Data_Buff 下标），满 512B 才写介质——**勿在未凑满时写**，否则数据错乱

### 6.6 写路径验证（V1.1 替代"格式化压力测试"）

V1.1 起格式化不可行（Windows 最小 FAT 卷 ≥32KB）。写路径验证改用 **WinHex 直接写扇区**：对 LBA0..LBA15 写入已知模式（如 0xA5/0x5A 交替、递增字节），读回比对一致性——覆盖多块、跨块边界、整块写入，等效于格式化压力测试。

---

## 7. 验证流程

### 7.1 编译验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | Keil Build (F7) | 0 Error(s), 0 Warning(s) | 待验证 |

### 7.2 USBTreeView / 设备管理器验证（回归）

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 烧录，插 USB | **Problem Code 保持消失** | C2 回归 ✅ |
| 2 | 磁盘容量 | 约 8KB（16 块 × 512B）不变 | C2 回归 ✅ |

### 7.3 写路径验证（核心验收，V1.1 替代"格式化为 FAT"）

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | WinHex → Open Disk → 选 STM32 SRAM Disk | 打开成功，容量 ~8KB | C2 回归 ✅ |
| 2 | **写扇区**：对 LBA0..LBA15 写入已知模式（如 0xA5/0x5A 交替、递增字节） | 写入无错误 | TR2-C2 ✅ |
| 3 | **读回比对**：重新打开扇区读取 | 与写入模式完全一致（覆盖多块、跨块边界、整块写） | TR2-C2 ✅ |
| 4 | 写全盘 0x00 → 断电重插 | 读回全 0x00（写入持久性，SRAM 断电前保持） | 佐证 ✅ |

> 补充（V1.1）：`format D: /FS:FAT` 对 8KB 卷报"卷对 FAT16/12 来说太大"——**Windows 最小 FAT 卷 ≥32KB 的已知限制**，非固件缺陷，格式化 + 文件读写顺延 TR3。

### 7.4 文件读写（V1.1 起顺延 TR3）

文件读写依赖格式化后的文件系统，而 Windows 无法格式化 ≤16KB 的 SRAM 卷 → **本项验收顺延 TR3**（W25Q32 Flash 4MB 满足最小卷要求）。TR3-03/04 承接：格式化为 FAT/FAT32 成功 → 创建/读取/修改/删除文件正常。

### 7.5 链接验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 查看 `.map` 文件 | `SCSI_Write10_Cmd` 来自 `usb_scsi.o`，`Write_Memory` 来自 `memory.o` | 待验证 |

---

## 8. 常见问题排查

### 8.1 格式化失败 / 报"Windows 无法完成格式化"

**原因**：WRITE10 未恢复（case 或 BOT_DATA_OUT 分支还在 `#if 0`）、EP2 未重新使能、或 `MAL_Write` 未恢复。

**排查**：确认 §5.2（SCSI_Write10_Cmd 已实现）、§5.3（两个分支已恢复）、§5.4（MAL_Write 已恢复）；检查 `SCSI_Write10_Cmd` 的 `SetEPRxStatus(ENDP2, EP_RX_VALID)` 是否在（§6.2）。

##区分两类情况**：
- **报"卷对 FAT16/12 来说太大"**（V1.1 确认）：Windows 最小 FAT 卷 ≥32KB，SRAM 20KB 无法满足——**预期限制，非缺陷**，格式化顺延 TR3。
- **报"Windows 无法完成格式化"且 WinHex 写扇区也失败**：写路径未生效。排查 WRITE10 是否恢复（§5.2/§5.3/§5.4）、EP2 使能时序（§6.2）。

**排查写路径**：用 WinHex 直接写扇区（§7.3）确认 `SCSI_Write10_Cmd` / `Mass_Storage_Out` 的 `BOT_DATA_OUT` / `Write_Memory` 的 `MAL_Write` 是否工作

**排查**：确认 §5.3(b) 分支恢复；确认 `Write_Memory` 每包结束有 `SetEPRxStatus(ENDP2, EP_RX_VALID)`（§6.2 第 2 处）。

### 8.3 编译报错：`undefined symbol SCSI_Write10_Cmd`

**原因**：`usb_bot.c` 恢复了 WRITE10 case，但 `usb_scsi.c` 未实现/未加入工程。

**修复**：确认函数已实现（§5.2），`usb_scsi.c` 已在 `project.uvprojx`。

### 8.4 写入内容与预期偏移不符（数据错乱）

**原因**：`MAL_Write` 的 `Memory_Offset` 被当作块号；或组包在未凑满 512B 时触发写介质。

**修复**：`Write_Memory` 用 `W_Offset - Mass_Block_Size[lun]` 字节偏移（§6.5）；确认 `if (!(W_Length % Mass_Block_Size[lun]))` 条件触发 `MAL_Write`。

### 8.5 格式化成功但文件系统损坏

**原因**：格式化过程中部分 WRITE10 丢失或写入错位（中断时序）。

**排查**：确认 EP2 使能时序（§6.2）与组包逻辑（§6.5）；重新格式化验证。

---

## 9. 与下一步的衔接

TR2-C2 完成后，PC 能对磁盘格式化为 FAT12 并读写文件，这是 TR2 阶段的**核心能力**。后续：

- **TR2打通写路径（WRITE10 → Write_Memory → MAL_Write），与 C1 的读路径对称完整，SRAM 虚拟 U 盘从"可读不可写"变为"**读写可用**"。后续：

- **TR2-C3**：`VERIFY10`（`BLKVFY` 宏已预定义）/`FORMAT_UNIT` 命令补全 + 安全弹出验证 + 断电数据丢失验证（框架 §8.3 步骤 4~5）
- **TR3（重点）**：承接 TR2 顺延的"格式化 + 文件读写"——MAL 层替换为 W25Q32 Flash（4MB，满足 Windows 最小 FAT 卷 ≥32KB 要求），新增 `flash.c/h` SPI 驱动 + `hw_config.c` SPI 初始化，BOT/SCSI/buffer 全部保持不变（框架 §9）。格式化 + 文件读写一步到位（TR3-03/04）

> **V1.1 修订**：C2 原预期"格式化 + 文件读写"受 Windows 最小 FAT 卷限制（≥32KB）顺延 TR3。C2 的价值不减——写路径是格式化的前提，TR3 换 Flash 后直接复用

*文档结束。*
