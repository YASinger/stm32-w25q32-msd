# TR2-S2 详细设计：SCSI 查询命令与 SRAM 介质层

## 修改记录

| 版本 | 日期 | 修改内容 | 修改人 |
|---|---|---|---|
| V1.0 | 2026-07-17 | 初始版本，基于 TR2-S1 已实现的 BOT 骨架 | Copilot |

---

## 文档信息

| 项目 | 内容 |
|---|---|
| 需求编号 | TR2-S2 |
| 需求描述 | SCSI 查询命令与 SRAM 介质层 |
| 验收标准 | INQUIRY/READ_CAPACITY 等查询命令正确响应，PC 显示约 8KB 可移动磁盘，可格式化 |
| 所属阶段 | TR2 — SRAM 虚拟 U 盘 |
| 文档版本 | V1.0 |
| 日期 | 2026-07-17 |

---

## 1. 需求分解

TR2-S1 实现了 BOT 骨架，但所有 SCSI 命令都返回 `CSW_CMD_FAILED`，导致 `USBSTOR.SYS` 判定设备不可用（`CM_PROB_FAILED_START`）。

TR2-S2 的任务是实现 Windows 枚举磁盘所需的**查询型 SCSI 命令**，并提供 **8KB SRAM 介质抽象层**，使设备以可移动磁盘形式出现，容量约 8KB，可被格式化。

```
USBSTOR 启动流程 (Windows):
  1. INQUIRY          → 设备返回厂商/产品名、可移除介质标志
  2. TEST_UNIT_READY  → 设备返回就绪
  3. READ_CAPACITY10  → 设备返回容量 (16 扇区 × 512B)
  4. READ_FORMAT_CAPACITIES → 设备返回格式化容量信息
  5. MODE_SENSE6/10   → 设备返回模式参数
  6. Windows 显示磁盘 → 用户可格式化
```

> **TR2-S2 的边界**：只实现查询型命令（无数据读写），READ10/WRITE10 在 TR2-S3 实现。格式化操作会发 WRITE10 写 FAT 表，所以 TR2-S2 阶段格式化可能会失败——但如果 SRAM 介质层的 `MAL_Read`/`MAL_Write` 已经实现（即使 SCSI 层还没调），格式化可以通过。**实际策略**：TR2-S2 把 `MAL_Read`/`MAL_Write` 一并实现，但 `CBW_Decode` 中 READ10/WRITE10 仍走 `CSW_CMD_FAILED`，格式化留到 TR2-S3 验证。

---

## 2. 涉及组件

```
┌─────────────────────────────────────────────────┐
│              PC (USBSTOR.SYS)                    │
│  INQUIRY / READ_CAPACITY / TEST_UNIT_READY ...   │
└────────────┬─────────────────────────┬──────────┘
             │ EP2 OUT (CBW)           │ EP1 IN (Data + CSW)
╔════════════╧═════════════════════════╧══════════╗
║  usb_bot (已有, 修改)                             ║
║  CBW_Decode() → switch(CBW.CB[0])                ║
║    → 调用 SCSI 命令处理函数                       ║
╠═══════════════════════════════════════════════════╣
║  usb_scsi (新建)                                  ║
║  INQUIRY / READ_CAPACITY10 / READ_FORMAT_CAPACITY  ║
║  TEST_UNIT_READY / REQUEST_SENSE / MODE_SENSE      ║
║  START_STOP_UNIT / Set_Scsi_Sense_Data              ║
╠═══════════════════════════════════════════════════╣
║  scsi_data (新建)                                  ║
║  Standard_Inquiry_Data / ReadCapacity10_Data ...    ║
╠═══════════════════════════════════════════════════╣
║  mass_mal (新建)                                   ║
║  MAL_Init / MAL_Read / MAL_Write / MAL_GetStatus    ║
║  → 8KB SRAM 缓冲区 (块大小 512B, 16 扇区)          ║
╚═══════════════════════════════════════════════════╝
```

| 组件 | 职责 | TR2-S2 中的角色 |
|---|---|---|
| **usb_scsi** (新建) | SCSI 命令处理 | 7 个查询命令的实现 + Sense Data 管理 |
| **scsi_data** (新建) | SCSI 响应数据 | INQUIRY / ReadCapacity 等静态数据表 |
| **mass_mal** (新建) | 介质访问层 | SRAM 读写 + 容量参数 |
| **usb_bot** (修改) | BOT 状态机 | `CBW_Decode()` 的 switch 替换为真实命令分发 |

---

## 3. SRAM 介质层 (`mass_mal.c` / `mass_mal.h`)

### 3.1 容量参数

| 参数 | 值 | 说明 |
|---|---|---|
| `Mass_Block_Size[0]` | 512 | 每扇区 512 字节 (FAT 最小单元) |
| `Mass_Block_Count[0]` | 16 | 16 个扇区 |
| `Mass_Memory_Size[0]` | 8192 | 8KB 总容量 |

### 3.2 SRAM 缓冲区

```c
/* 8KB SRAM 缓冲区 — 用作虚拟磁盘的存储介质 */
#define MAL_RAM_SIZE    8192
static uint8_t SRAM_Buffer[MAL_RAM_SIZE];
```

> 使用 `static` 限定，仅 `mass_mal.c` 内部可见。STM32F103C8 有 20KB SRAM，8KB 缓冲区占用约 40%，剩余 12KB 供程序使用。

### 3.3 接口实现

```c
uint16_t MAL_Init(uint8_t lun)
{
    if (lun > 0) return MAL_FAIL;
    memset(SRAM_Buffer, 0, MAL_RAM_SIZE);
    Mass_Block_Size[0]  = 512;
    Mass_Block_Count[0] = 16;
    Mass_Memory_Size[0] = 8192;
    return MAL_OK;
}

uint16_t MAL_Read(uint8_t lun, uint32_t Memory_Offset,
                  uint32_t *Readbuff, uint16_t Transfer_Length)
{
    if (lun > 0) return MAL_FAIL;
    memcpy(Readbuff, &SRAM_Buffer[Memory_Offset], Transfer_Length);
    return MAL_OK;
}

uint16_t MAL_Write(uint8_t lun, uint32_t Memory_Offset,
                   uint32_t *Writebuff, uint16_t Transfer_Length)
{
    if (lun > 0) return MAL_FAIL;
    memcpy(&SRAM_Buffer[Memory_Offset], Writebuff, Transfer_Length);
    return MAL_OK;
}

uint16_t MAL_GetStatus(uint8_t lun)
{
    if (lun > 0) return MAL_FAIL;
    return MAL_OK;   /* SRAM 始终就绪 */
}
```

### 3.4 全局变量

```c
uint32_t Mass_Memory_Size[2];    /* [0] = 8192 */
uint32_t Mass_Block_Size[2];      /* [0] = 512 */
uint32_t Mass_Block_Count[2];    /* [0] = 16 */
```

### 3.5 初始化时机

在 `MASS_Reset()` 中调用 `MAL_Init(0)`：

```c
/* usb_prop.c MASS_Reset() 末尾追加 */
MAL_Init(0);
```

---

## 4. SCSI 响应数据 (`scsi_data.c`)

### 4.1 INQUIRY Data (36 字节)

```
偏移  字段                    值                       说明
0     PeripheralQual+Type    0x00                     Direct Access (块设备)
1     RMB                    0x80                     可移除介质
2     Version                0x02                     不声称符合标准
3     ResponseFormat         0x02                     SCSI-2 响应格式
4     AdditionalLength       32 (36-4)                后续数据长度
5-7   保留                   0x00
8-15  Vendor Identification "STM     "                8 字节
16-31 Product Identification "W25Q32 Flash Disk "     16 字节
32-35 ProductRevisionLevel  "1.0 "                    4 字节
```

> 厂商改为 `"STM     "`（8 字节，空格填充），产品改为 `"W25Q32 Flash Disk "`（16 字节，空格填充），版本 `"1.0 "`。

### 4.2 ReadCapacity10 Data (8 字节)

```
偏移  字段              运行时填充
0-3   Last LBA          Mass_Block_Count[0] - 1 = 15 (大端)
4-7   Block Length       Mass_Block_Size[0] = 512 (大端)
```

> 运行时由 `SCSI_ReadCapacity10_Cmd()` 填充，不是静态值。

### 4.3 ReadFormatCapacity Data (12 字节)

```
偏移  字段              值
0-3   Header            0x00, 0x00, 0x00, 0x08 (Capacity List Length=8)
4-7   Block Count        Mass_Block_Count[0] = 16 (大端, 运行时填充)
8-11  Block Length       0x02(格式化), 0x00, 0x02, 0x00 (= 512, 运行时填充)
```

### 4.4 ModeSense6 Data (4 字节)

```c
uint8_t Mode_Sense6_data[] = { 0x03, 0x00, 0x00, 0x00 };
```

### 4.5 ModeSense10 Data (8 字节)

```c
uint8_t Mode_Sense10_data[] = { 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
```

### 4.6 RequestSense Data (18 字节)

```c
uint8_t Scsi_Sense_Data[] = {
    0x70,       /* Response Code (current error) */
    0x00,       /* Segment Number */
    NO_SENSE,   /* Sense Key (运行时由 Set_Scsi_Sense_Data 修改) */
    0x00, 0x00, 0x00, 0x00,   /* Information */
    0x0A,       /* Additional Sense Length = 10 */
    0x00, 0x00, 0x00, 0x00,   /* Command Specific Information */
    NO_SENSE,   /* ASC (运行时修改) */
    0x00,       /* ASCQ */
    0x00,       /* FRUC */
    0x00, 0x00  /* Sense Key Specific */
};
```

### 4.7 Page00 Inquiry Data (5 字节)

```c
uint8_t Page00_Inquiry_Data[] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
```

> 当 INQUIRY 的 Evpd 位为 1 时返回此数据（VPD 页列表，空）。

---

## 5. SCSI 命令处理 (`usb_scsi.c` / `usb_scsi.h`)

### 5.1 命令码定义

```c
#define SCSI_TEST_UNIT_READY        0x00
#define SCSI_REQUEST_SENSE          0x03
#define SCSI_FORMAT_UNIT            0x04
#define SCSI_INQUIRY                0x12
#define SCSI_MODE_SENSE6            0x1A
#define SCSI_MODE_SENSE10           0x5A
#define SCSI_START_STOP_UNIT        0x1B
#define SCSI_ALLOW_MEDIUM_REMOVAL   0x1E
#define SCSI_READ_FORMAT_CAPACITIES 0x23
#define SCSI_READ_CAPACITY10        0x25
#define SCSI_READ10                 0x28
#define SCSI_WRITE10                0x2A
#define SCSI_VERIFY10               0x2F
```

### 5.2 命令实现

#### `SCSI_Inquiry_Cmd(lun)` — 设备信息查询

```
if (CBW.CB[1] & 0x01)   /* Evpd 位: 请求 VPD 页 */
    → Transfer_Data_Request(Page00_Inquiry_Data, 5)
else
    → Transfer_Data_Request(Standard_Inquiry_Data, min(CBW.CB[4], 36))
```

#### `SCSI_ReadCapacity10_Cmd(lun)` — 容量查询

```
if (MAL_GetStatus(lun) != 0)
    → Set_CSW(FAIL) + Bot_Abort(IN)
else
    ReadCapacity10_Data[0-3] = Mass_Block_Count[0] - 1  (大端, 最后一个 LBA)
    ReadCapacity10_Data[4-7] = Mass_Block_Size[0]        (大端, 块大小)
    → Transfer_Data_Request(ReadCapacity10_Data, 8)
```

> **注意**：READ_CAPACITY10 返回的是**最后一个 LBA 号**（= 块数 - 1），不是总块数。

#### `SCSI_ReadFormatCapacity_Cmd(lun)` — 格式化容量查询

```
if (MAL_GetStatus(lun) != 0)
    → Set_CSW(FAIL) + Bot_Abort(IN)
else
    ReadFormatCapacity_Data[4-7] = Mass_Block_Count[0]   (大端, 块数)
    ReadFormatCapacity_Data[8-11] = 0x02, 0x00, 0x02, 0x00  (格式化, 512B)
    → Transfer_Data_Request(ReadFormatCapacity_Data, 12)
```

#### `SCSI_TestUnitReady_Cmd(lun)` — 就绪检查

```
if (MAL_GetStatus(lun) != 0)
    → Set_CSW(FAIL) + Bot_Abort(IN)
else
    → Set_CSW(PASS, SEND_CSW_ENABLE)
```

#### `SCSI_RequestSense_Cmd(lun)` — 查询错误信息

```
→ Transfer_Data_Request(Scsi_Sense_Data, min(CBW.CB[4], 18))
```

#### `SCSI_ModeSense6_Cmd(lun)` / `SCSI_ModeSense10_Cmd(lun)`

```
→ Transfer_Data_Request(Mode_Sense6_data, 4)   /* 或 Mode_Sense10_data, 8 */
```

#### `SCSI_Start_Stop_Unit_Cmd(lun)` — 启停/弹出

```
→ Set_CSW(PASS, SEND_CSW_ENABLE)
```

> TR2-S4 将完善弹出处理。

#### `Set_Scsi_Sense_Data(lun, Sens_Key, Asc)`

```
Scsi_Sense_Data[2]  = Sens_Key
Scsi_Sense_Data[12] = Asc
```

### 5.3 READ10 / WRITE10 / VERIFY10 (TR2-S3 才实现)

TR2-S2 阶段这三个命令仍走 `Set_CSW(CSW_CMD_FAILED, SEND_CSW_ENABLE)`，在 `CBW_Decode` 的 switch 中不添加 case，落入 default。

---

## 6. CBW_Decode 修改 (`usb_bot.c`)

### 6.1 当前状态 (TR2-S1)

```c
/* TR2-S1: 所有命令返回 FAIL */
Set_CSW(CSW_CMD_FAILED, SEND_CSW_ENABLE);
```

### 6.2 修改后 (TR2-S2)

```c
switch (CBW.CB[0]) {
case SCSI_TEST_UNIT_READY:
    SCSI_TestUnitReady_Cmd(CBW.bLUN);
    break;
case SCSI_REQUEST_SENSE:
    SCSI_RequestSense_Cmd(CBW.bLUN);
    break;
case SCSI_INQUIRY:
    SCSI_Inquiry_Cmd(CBW.bLUN);
    break;
case SCSI_START_STOP_UNIT:
    SCSI_Start_Stop_Unit_Cmd(CBW.bLUN);
    break;
case SCSI_ALLOW_MEDIUM_REMOVAL:
    SCSI_Start_Stop_Unit_Cmd(CBW.bLUN);  /* 复用 */
    break;
case SCSI_MODE_SENSE6:
    SCSI_ModeSense6_Cmd(CBW.bLUN);
    break;
case SCSI_MODE_SENSE10:
    SCSI_ModeSense10_Cmd(CBW.bLUN);
    break;
case SCSI_READ_FORMAT_CAPACITIES:
    SCSI_ReadFormatCapacity_Cmd(CBW.bLUN);
    break;
case SCSI_READ_CAPACITY10:
    SCSI_ReadCapacity10_Cmd(CBW.bLUN);
    break;
case SCSI_FORMAT_UNIT:
    SCSI_Start_Stop_Unit_Cmd(CBW.bLUN);  /* 暂复用, TR2-S3 实现 */
    break;

/* TR2-S3 才实现 */
case SCSI_READ10:
case SCSI_WRITE10:
case SCSI_VERIFY10:
default:
    Set_CSW(CSW_CMD_FAILED, SEND_CSW_ENABLE);
    break;
}
```

### 6.3 include 修改

`usb_bot.c` 顶部新增：

```c
#include "usb_scsi.h"
#include "mass_mal.h"
```

---

## 7. 数据流：INQUIRY 全过程

```
时间线      PC (USBSTOR)                              STM32 侧
─────────────────────────────────────────────────────────────────
T1          发送 INQUIRY CBW (31B)
            bRequest=0x12, CB[4]=36                  EP2 OUT 中断
                                                       → Mass_Storage_Out()
                                                       → CBW_Decode()
                                                       → CBW.CB[0] = 0x12
                                                       → SCSI_Inquiry_Cmd(0)
                                                         → Evpd=0
                                                         → Transfer_Data_Request(
                                                             Standard_Inquiry_Data, 36)
                                                           → USB_SIL_Write(EP1, ..., 36)
                                                           → Bot_State = BOT_DATA_IN_LAST
                                                           → CSW.bStatus = PASS
T2          发送 IN 令牌                              EP1 IN 中断
            ← 收到 36 字节 INQUIRY 数据                 → Mass_Storage_In()
                                                       → Bot_State == BOT_DATA_IN_LAST
                                                       → Set_CSW(PASS, ENABLE)
                                                         → CSW 写入 PMA
                                                         → Bot_State = BOT_CSW_Send
T3          发送 IN 令牌
            ← 收到 13 字节 CSW (PASS)                  EP1 IN 中断
                                                       → Mass_Storage_In()
                                                       → Bot_State == BOT_CSW_Send
                                                       → Bot_State = BOT_IDLE
                                                       → SetEPRxStatus(EP2, VALID)
T4          解析 INQUIRY 响应:
            "STM", "W25Q32 Flash Disk"
            RMB=0x80 (可移除)
            → 设备类型识别完成
```

---

## 8. 文件清单：需新建 / 需修改

### 8.1 需新建的文件

| 文件 | 说明 |
|---|---|
| `src/usb_scsi.c` | SCSI 命令处理函数 |
| `inc/usb_scsi.h` | SCSI 命令码、Sense Key、函数声明 |
| `src/scsi_data.c` | SCSI 响应数据表 |
| `src/mass_mal.c` | SRAM 介质访问层 |
| `inc/mass_mal.h` | MAL 接口声明 |

### 8.2 需修改的文件

| 文件 | 修改内容 |
|---|---|
| `src/usb_bot.c` | `CBW_Decode()` switch 替换为真实命令分发；新增 `#include "usb_scsi.h"` / `#include "mass_mal.h"` |
| `src/usb_prop.c` | `MASS_Reset()` 末尾追加 `MAL_Init(0)` |
| `project.uvprojx` | 添加 5 个新文件到编译列表 |

### 8.3 不变的文件

| 文件 | 说明 |
|---|---|
| `src/usb_endp.c` | 端点回调不变 |
| `src/usb_istr.c` | 中断分发不变 |
| `inc/usb_conf.h` | 端点配置不变 |
| `inc/usb_bot.h` | BOT 结构体不变 |

---

## 9. 验证方法

| 步骤 | 操作 | 预期结果 | 对应需求 |
|---|---|---|---|
| 1 | 编译烧录固件 | Keil 0 Error 0 Warning | — |
| 2 | USB 插入 PC | 设备管理器**无黄色感叹号** | TR2-S2 ✅ |
| 3 | 打开"此电脑" | 出现可移动磁盘，容量约 8KB | TR2-S2 ✅ |
| 4 | 右键属性 | 设备名含 "W25Q32 Flash Disk" | TR2-S2 ✅ |
| 5 | 尝试格式化 | **可能失败**（WRITE10 未实现，TR2-S3 才能格式化） | 预期 |
| 6 | USBTreeView | 2 pipe 正常，无 Problem Code | TR2-S2 ✅ |

> **TR2-S2 最小验收**：步骤 1~4 通过。设备无感叹号 + 可移动磁盘出现 + 容量 8KB = SCSI 查询命令正确。

---

## 10. 常见问题排查

| 现象 | 可能原因 | 排查方向 |
|---|---|---|
| 仍有黄色感叹号 | INQUIRY 数据非法 | 检查 `Standard_Inquiry_Data` 的 RMB=0x80、AdditionalLength=32 |
| | `CBW_Decode` switch 未替换 | 确认 `usb_bot.c` 已 include `usb_scsi.h` |
| | `MAL_Init` 未调用 | 检查 `MASS_Reset()` 是否追加了 `MAL_Init(0)` |
| 容量显示 0 | `ReadCapacity10_Data` 未填充 | 检查 `SCSI_ReadCapacity10_Cmd` 是否大端填充 |
| | `Mass_Block_Count` 为 0 | 检查 `MAL_Init` 是否设置了 `Mass_Block_Count[0]=16` |
| 容量显示很大 | `Mass_Block_Count` 越界 | 确认值为 16，不是 1600 或更大 |
| HardFault | SRAM 缓冲区溢出 | 确认 `MAL_RAM_SIZE = 8192`，读写偏移不越界 |
| 格式化失败 | WRITE10 未实现 (TR2-S3) | 预期行为，TR2-S3 修复 |

---

## 11. 与其他需求的关系

| 需求 | 关系 |
|---|---|
| TR2-S1 (BOT 状态机) | **前置**。TR2-S1 的 `CBW_Decode` switch 为 TR2-S2 提供命令分发入口 |
| TR2-S3 (SCSI 读写命令) | **后继**。TR2-S2 的 `MAL_Read`/`MAL_Write` 为 TR2-S3 的 READ10/WRITE10 提供介质访问 |
| TR2-S4 (安全弹出) | **后继**。`SCSI_Start_Stop_Unit_Cmd` 在 TR2-S4 完善 |

---

*文档结束。*
