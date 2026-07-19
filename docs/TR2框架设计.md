# TR2 框架设计 — SRAM 虚拟 U 盘

## 文档信息

| 项目 | 内容 |
|---|---|
| 项目名称 | stm32-w25q32-msd |
| 文档类型 | TR2 阶段框架设计 |
| 版本 | V1.0 |
| 日期 | 2026-07-19 |
| 前置文档 | `项目框架设计.md`（整体分层与组件定义）、`TR1框架设计.md` |

---

## 1. 设计目标

让 PC 能对 STM32 模拟的 USB 大容量存储设备进行格式化和文件读写，存储介质为 SRAM（断电丢失）。Problem Code 10 消失，USBSTOR 驱动完全加载，磁盘驱动器（disk.sys）挂载。

**与 `项目框架设计.md` 的关系**：本文档是整体框架中 §6.2 TR2 的细化展开，把"SRAM 虚拟 U 盘"这一个粗粒度目标，拆成三个子阶段、每条需求对应一次可独立提交的代码改动。

**与 TR1 的衔接**：TR1 完成后，EP1/EP2 的回调指向 `NOP_Process`，USBSTOR 驱动发 SCSI 命令时设备无响应（Problem Code 10）。TR2 接管 EP1/EP2 回调，实现 BOT 状态机和 SCSI 命令，让 PC 能读写存储介质。

---

## 2. 拆分原则

### 2.1 TR2 与 TR1 的拆分差异

TR1 的拆分难点是"USB 枚举是连续流程，组件缺失导致整体失败"。TR2 的拆分难点是"BOT/SCSI/buffer/MAL 四层高度耦合，但可按功能层分离"。

TR2 的四层架构：

```
usb_endp.c (端点回调入口)
    ↓
usb_bot.c  (BOT 状态机: CBW 解码 / CSW 生成 / 状态流转)
    ↓
usb_scsi.c (SCSI 命令: INQUIRY / READ_CAPACITY / READ10 / WRITE10 ...)
    ↓               ↓
scsi_data.c      memory.c (缓冲调度: 64B↔512B 拆包组包)
(静态数据)           ↓
                 mass_mal.c (介质访问层: SRAM 数组)
```

### 2.2 本文档的拆分方式

沿用 TR1 的**骨架先行 + 纵向切片 + 横向补全**三步：

| 子阶段 | 比喻 | 核心特征 |
|---|---|---|
| **TR2-A 骨架** | 打地基 | 四层组件代码全部到位、能编译、CBW/CSW 收发机制通，但 SCSI 命令全返回 INVALID |
| **TR2-B 纵向切片** | 立框架 | SCSI 查询命令接通，Problem Code 10 消失，PC 见 8KB 磁盘 |
| **TR2-C 逐层补全** | 装修 | READ10/WRITE10 逐步实现，每步插 USB 都有可观测差异 |

**关键约束**：每条需求对应一次 git 提交，提交的 diff 范围明确，且提交后有确定的可观测结果。

---

## 3. 子阶段定义

### 3.1 TR2-A — BOT 协议栈骨架可编译运行

**目标**：让 BOT/SCSI/buffer/MAL 四层组件代码全部到位、能编译。EP1/EP2 回调从 `NOP_Process` 替换为真实 BOT 函数，CBW 能被接收并返回 CSW（即使 CSW 说"命令不支持"）。

| 编号 | 内容 | 可观测结果 | 涉及组件 |
|---|---|---|---|
| TR2-A1 | `mass_mal.c/h` SRAM 介质层（8KB SRAM 数组 + Init/Read/Write/GetStatus） | 编译通过 | mal |
| TR2-A2 | `scsi_data.c/h` SCSI 静态数据（Inquiry/SenseData/ModeSense/ReadCapacity/ReadFormatCapacity） | 编译通过 | scsi_data |
| TR2-A3 | `usb_bot.c/h` BOT 状态机骨架 + `usb_endp.c` 端点回调接管（CBW 解码 + CSW 返回，SCSI 全 INVALID） + `usb_conf.h` 回调宏切换 | EP1/EP2 回调不再是 NOP，CBW 能被接收 | bot, endp |
| TR2-A4 | `memory.c/h` 缓冲调度骨架（Read_Memory/Write_Memory 框架，暂不调用 MAL） | 编译通过 | buffer |

**阶段产物**：可烧录固件，USBSTOR 驱动发 SCSI 命令时设备能接收 CBW 并返回 CSW（CMD_FAILED），不再超时。

### 3.2 TR2-B — SCSI 查询命令走通（纵向切片）

**目标**：实现 SCSI 查询命令，让 USBSTOR 驱动完全加载，PC 显示 8KB 可移动磁盘。

| 编号 | 内容 | 可观测结果 | 涉及组件 |
|---|---|---|---|
| TR2-B1 | `usb_scsi.c/h` SCSI 查询命令实现（INQUIRY/READ_CAPACITY/TEST_UNIT_READY/REQUEST_SENSE/MODE_SENSE6/10/READ_FORMAT_CAPACITIES/START_STOP_UNIT/ALLOW_MEDIUM_REMOVAL） + `usb_bot.c` CBW_Decode 接入 + Transfer_Data_Request 数据发送 | **Problem Code 10 消失**，USBSTOR + disk.sys 驱动完全加载，PC 显示约 8KB 可移动磁盘 | scsi, bot |

**阶段产物**：PC 设备管理器出现"磁盘驱动器"，可查看容量（8KB / 16 块 × 512B），但无法格式化（WRITE10 未实现）。

> **关键**：TR2-B1 完成时 Problem Code 10 消失。这是 TR2 阶段的第一个里程碑——从"驱动启动失败"到"磁盘可见"。

### 3.3 TR2-C — 读写命令走通（横向补全）

**目标**：实现 READ10/WRITE10 读写命令，让 PC 能格式化和文件操作。**每条需求都是一次独立提交**，且每次提交后都有可观测差异。

| 编号 | 内容 | 可观测结果 | 涉及组件 |
|---|---|---|---|
| TR2-C1 | `memory.c` Read_Memory 实现 + `usb_scsi.c` SCSI READ10 实现 | 可读取磁盘内容（格式化前的 0xFF 填充或格式化后的 MBR/FAT） | buffer, scsi |
| TR2-C2 | `memory.c` Write_Memory 实现 + `usb_scsi.c` SCSI WRITE10 实现 | **可格式化为 FAT**，可创建/写入/读取/删除文件 | buffer, scsi |
| TR2-C3 | 其余 SCSI 命令补全（VERIFY10 等） + 安全弹出验证 | 安全弹出无错误提示，断电后数据丢失（SRAM 特性） | scsi |

**阶段产物**：完整 SRAM 虚拟 U 盘，可格式化、可读写文件，断电数据丢失。TR2 阶段全部完成。

---

## 4. 与原需求列表的映射

原 `需求列表.md` 的 TR2-S1~S4 描述的是"最终状态该满足什么"，作为**验收清单**仍有价值。本文档的 TR2-A/B/C 描述的是**开发任务**，按 A→B→C 顺序提交，最终用原 TR2-S1~S4 检查。

| 开发任务 | 对应验收需求 | 关系 |
|---|---|---|
| TR2-A1~A4 | 无（骨架不要求 PC 见磁盘） | 前置基础 |
| TR2-B1 | TR2-S1（BOT 状态机与端点接管，设备无感叹号） | B1 完成即满足 TR2-S1 |
| TR2-B1 | TR2-S2 部分（INQUIRY/READ_CAPACITY 正确响应，PC 显示 8KB） | B1 完成即满足 TR2-S2 的查询部分 |
| TR2-C1~C2 | TR2-S2 部分（文件读写） + TR2-S3（READ10/WRITE10 走通） | C2 完成即满足 TR2-S3 |
| TR2-C3 | TR2-S4（安全弹出与断电验证） | 直接对应 |

---

## 5. 组件清单（TR2 阶段产出文件）

### 5.1 TR2 阶段涉及的全部文件

| 文件 | 所属组件 | 所属子阶段 | 说明 |
|---|---|---|---|
| `src/mass_mal.c` | mal | TR2-A1 | SRAM 介质访问层 |
| `inc/mass_mal.h` | mal | TR2-A1 | MAL 接口声明 |
| `src/scsi_data.c` | scsi_data | TR2-A2 | SCSI 静态响应数据 |
| `inc/scsi_data.h` | scsi_data | TR2-A2 | 数据声明 |
| `src/usb_bot.c` | bot | TR2-A3 / TR2-B1 / TR2-C1~C2 | BOT 协议状态机 |
| `inc/usb_bot.h` | bot | TR2-A3 | BOT 接口声明 |
| `src/usb_endp.c` | endp | TR2-A3 | 端点回调桥接 |
| `inc/usb_conf.h` | endpoint | TR2-A3 | EP1_IN/EP2_OUT 回调宏切换 |
| `src/memory.c` | buffer | TR2-A4 / TR2-C1~C2 | 读写缓冲调度 |
| `inc/memory.h` | buffer | TR2-A4 | 接口声明 |
| `src/usb_scsi.c` | scsi | TR2-B1 / TR2-C1~C3 | SCSI 命令处理 |
| `inc/usb_scsi.h` | scsi | TR2-B1 | 接口声明 |

### 5.2 TR2 阶段不修改的文件

TR1 阶段的 `usb_desc.c/h`、`usb_prop.c/h`、`usb_pwr.c/h`、`usb_istr.c/h`、`hw_config.c/h`、`main.c` 在 TR2 阶段保持不变。TR2 只新增 6 个组件，不改动 TR1 的已有组件（除了 `usb_conf.h` 的回调宏切换）。

---

## 6. 关键技术决策

### 6.1 SRAM 磁盘容量

STM32F103C8 有 20KB SRAM，分配 **8KB** 给磁盘存储：

```
SRAM 布局:
  0x20000000 ~ 0x20001FFF  8KB  磁盘存储 (sram_disk[8192])
  0x20002000 ~ 0x20004FFF  12KB 程序栈/堆/全局变量
```

- 块大小：512 字节（USB MSD 标准）
- 块数：8192 / 512 = 16 块
- 最后 LBA：15（0-based）
- 格式化：FAT12（最小 FAT 文件系统，16 扇区可格式化）

### 6.2 EP1/EP2 回调切换机制

TR1 阶段 `usb_conf.h` 中：
```c
#define EP1_IN_Callback   NOP_Process
#define EP2_OUT_Callback  NOP_Process
```

TR2 阶段需要替换为真实函数。方法：在 `usb_conf.h` 中注释掉这两个宏，在 `usb_endp.c` 中定义同名函数：

```c
/* usb_conf.h */
//#define  EP1_IN_Callback   NOP_Process   /* TR2: 替换为 usb_endp.c 中的真实函数 */
//#define  EP2_OUT_Callback  NOP_Process   /* TR2: 替换为 usb_endp.c 中的真实函数 */

/* usb_endp.c */
void EP1_IN_Callback(void)  { Mass_Storage_In(); }
void EP2_OUT_Callback(void) { Mass_Storage_Out(); }
```

`usb_istr.c` 的 `pEpInt_IN[0]` / `pEpInt_OUT[0]` 初始化为 `EP1_IN_Callback` / `EP1_OUT_Callback` 宏——宏被注释后，这两个名字变成未定义符号，需要在 `usb_endp.c` 中提供函数定义。

### 6.3 BOT 状态机

```
BOT_IDLE ──CBW──→ 命令分发
                    ├─ 非数据命令 ──→ Transfer_Data_Request ──→ BOT_DATA_IN_LAST
                    ├─ READ10 ──→ Read_Memory ──→ BOT_DATA_IN ──→ BOT_DATA_IN_LAST
                    └─ WRITE10 ──→ BOT_DATA_OUT ──→ Write_Memory ──→ Set_CSW
                                                                                  ↓
BOT_DATA_IN_LAST ──→ Set_CSW ──→ BOT_CSW_Send ──→ IN中断 ──→ BOT_IDLE
BOT_ERROR ──→ IN中断 ──→ BOT_IDLE
```

### 6.4 CBW/CSW 结构

**CBW（Command Block Wrapper，31 字节）**：
```
偏移  长度  字段
0     4    dCBWSignature (0x43425355 = "USBC")
4     4    dCBWTag (主机生成，CSW 原样返回)
8     4    dCBWDataTransferLength (数据阶段总字节数)
12    1    bmCBWFlags (bit7: 0=OUT, 1=IN)
13    1    bCBWLUN (逻辑单元号)
14    6    CBWCB[16] 的前 6 字节保留
15    1    bCBWCBLength (命令块长度，通常 10 或 16)
16    16   CBWCB[16] (SCSI 命令块)
```

**CSW（Command Status Wrapper，13 字节）**：
```
偏移  长度  字段
0     4    dCSWSignature (0x53425355 = "USBS")
4     4    dCSWTag (原样返回 CBW 的 dCBWTag)
8     4    dCSWDataResidue (剩余未传输字节数)
12    1    bCSWStatus (0=Passed, 1=Failed, 2=Phase Error)
```

### 6.5 缓冲调度机制

USB 端点包大小 64 字节，逻辑块大小 512 字节。`memory.c` 负责在两者之间拆包/组包：

**读取（READ10）**：
1. `MAL_Read()` 从 SRAM 读 512 字节到 `Data_Buffer[512]`
2. 分 8 次（512/64=8）通过 `USB_SIL_Write(EP1_IN, &Data_Buffer[offset], 64)` 发送
3. 每次 EP1 IN 中断回调 `Read_Memory()` 发送下一包

**写入（WRITE10）**：
1. 每次 EP2 OUT 中断回调 `Write_Memory()` 从 `Bulk_Data_Buff[64]` 拷贝到 `Data_Buffer[512]`
2. 积累满 512 字节后 `MAL_Write()` 写入 SRAM
3. 全部写完后发 CSW

---

## 7. 提交节奏建议

```
TR2-A1: mass_mal SRAM 介质层 (8KB 数组 + Init/Read/Write/GetStatus)
TR2-A2: scsi_data SCSI 静态响应数据
TR2-A3: usb_bot BOT 状态机骨架 + usb_endp 端点回调接管
TR2-A4: memory 缓冲调度骨架
TR2-B1: usb_scsi SCSI 查询命令 + bot CBW_Decode 接入, Problem Code 10 消失
TR2-C1: SCSI READ10 + memory Read_Memory 实现, 可读取磁盘内容
TR2-C2: SCSI WRITE10 + memory Write_Memory 实现, 可格式化/读写文件
TR2-C3: 其余 SCSI 命令补全 + 安全弹出验证
```

共 8 次提交，每次 diff 范围明确，可独立 review。

---

## 8. 验证流程

### 8.1 TR2-A 验证（骨架）

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | Keil 编译 | 0 Error, 0 Warning | TR2-A1~A4 ✅ |
| 2 | 烧录，USBTreeView 查看 | 与 TR1-C4 一致（Problem Code 10） | TR2-A3 ✅ |
| 3 | USBTreeView Used Endpoints | 3（EP0+EP1+EP2） | TR2-A3 ✅ |

### 8.2 TR2-B 验证（查询命令）

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 烧录，USBTreeView 查看 | **Problem Code 消失** | TR2-B1 ✅ |
| 2 | USBTreeView String Descriptors | **完整可读**（Problem Code 消失后） | TR2-B1 ✅ |
| 3 | 设备管理器 → 磁盘驱动器 | 出现 "STM W25Q32 Flash Dis USB Device" | TR2-B1 ✅ |
| 4 | 查看磁盘容量 | 约 8KB（16 块 × 512B） | TR2-B1 ✅ |

### 8.3 TR2-C 验证（读写命令）

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 用工具读取磁盘内容（如 WinHex） | 能读到 0xFF 或 MBR/FAT 数据 | TR2-C1 ✅ |
| 2 | 格式化为 FAT | 格式化成功，容量显示 ~8KB | TR2-C2 ✅ |
| 3 | 创建文件 → 写入内容 → 读取比对 | 内容正确 | TR2-C2 ✅ |
| 4 | 删除文件 | 删除成功 | TR2-C2 ✅ |
| 5 | 安全弹出 | 无错误提示 | TR2-C3 ✅ |
| 6 | 拔掉 USB 重新插入 | 数据丢失（SRAM 特性），需重新格式化 | TR2-C3 ✅ |

---

## 9. 与 TR3 的衔接

TR2 完成后，MAL 层用 SRAM 数组模拟存储。TR3 只需替换 MAL 内部实现（SRAM → W25Q32 Flash），新增 `flash.c/h` SPI Flash 驱动和 `hw_config.c` 的 SPI 初始化。BOT/SCSI/buffer 全部保持不变。

---

*文档结束。*
