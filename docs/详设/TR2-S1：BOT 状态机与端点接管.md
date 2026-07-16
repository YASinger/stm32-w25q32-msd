# TR2-S1 详细设计：BOT 状态机与端点接管

## 修改记录

| 版本 | 日期 | 修改内容 | 修改人 |
|---|---|---|---|
| V1.0 | 2026-07-16 | 初始版本，基于 TR1 v0.2.2 已通过的代码 | Copilot |

---

## 文档信息

| 项目 | 内容 |
|---|---|
| 需求编号 | TR2-S1 |
| 需求描述 | BOT 状态机与端点接管 |
| 验收标准 | EP1/EP2 由 NOP 替换为真实 BOT 状态机，能接收 CBW、返回 CSW，设备无感叹号 |
| 所属阶段 | TR2 — SRAM 虚拟 U 盘 |
| 文档版本 | V1.0 |
| 日期 | 2026-07-16 |

---

## 1. 需求分解

TR1 完成了 USB 枚举，设备以 Mass Storage 身份被 Windows 识别。但 EP1(Bulk IN) 和 EP2(Bulk OUT) 的中断回调仍指向 `NOP_Process`，无法收发 SCSI 命令数据。TR2-S1 的任务是：

```
主机发 CBW (31B) → EP2 OUT 中断 → Mass_Storage_Out() → CBW_Decode()
                                                         → 解析命令 (TR2-S2 才真正处理)
                                                         → 暂时返回 CSW (CMD_FAILED)
主机读 CSW (13B) ← EP1 IN 中断  ← Mass_Storage_In()
```

> **TR2-S1 的边界**：只要 BOT 状态机能走通"收 CBW → 回 CSW"这个骨架即可，SCSI 命令的具体处理（INQUIRY/READ_CAPACITY 等）在 TR2-S2 实现。本步中所有 SCSI 命令都会走 `Set_CSW(CSW_CMD_FAILED, SEND_CSW_ENABLE)` 路径——Windows 会认为命令失败，但 BOT 协议层是通的，设备不会出现感叹号。

---

## 2. 涉及组件

```
┌─────────────────────────────────────────────────┐
│                    PC (USBSTOR.SYS)              │
│   发送 CBW, 读取 CSW                             │
└────────────┬───────────────────┬─────────────────┘
             │ EP2 OUT (CBW)     │ EP1 IN (CSW/Data)
╔════════════╧═══════════════════╧═════════════════╗
║  usb_istr (中断入口)                               ║
║  pEpInt_OUT[1] → EP2_OUT_Callback                 ║
║  pEpInt_IN[0]  → EP1_IN_Callback                  ║
╠═══════════════════════════════════════════════════╣
║  usb_endp (端点回调, 新建)                          ║
║  EP1_IN_Callback  → Mass_Storage_In()             ║
║  EP2_OUT_Callback → Mass_Storage_Out()             ║
╠═══════════════════════════════════════════════════╣
║  usb_bot (BOT 状态机, 新建)                         ║
║  Mass_Storage_In/Out, CBW_Decode, Set_CSW, Bot_Abort║
╠═══════════════════════════════════════════════════╣
║  usb_prop (设备属性, 修改)                          ║
║  MASS_Reset 初始化 BOT 状态                         ║
║  MASS_NoData_Setup 接入 BOT 复位                    ║
╠═══════════════════════════════════════════════════╣
║  usb_conf (端点配置, 修改)                          ║
║  EP1_IN_Callback / EP2_OUT_Callback 解除 NOP 绑定   ║
╚═══════════════════════════════════════════════════╝
```

| 组件 | 职责 | TR2-S1 中的角色 |
|---|---|---|
| **usb_bot** (新建) | BOT 状态机 | CBW 接收/解码、CSW 发送、状态流转、异常 STALL |
| **usb_endp** (新建) | 端点回调 | EP1 IN / EP2 OUT 中断入口，转发到 BOT 状态机 |
| **usb_conf** (修改) | 端点配置 | 解除 EP1_IN/EP2_OUT 的 NOP_Process 宏绑定 |
| **usb_prop** (修改) | 设备属性 | MASS_Reset 初始化 BOT 状态；Bulk-Only Reset 接入 BOT |
| **usb_istr** (不变) | 中断服务 | 回调表不变，引用的宏名不变，但宏的值变了 |

---

## 3. 各模块详细设计

### 3.1 BOT 状态机 (`usb_bot.c` / `usb_bot.h`)

#### 3.1.1 核心数据结构

```c
/* CBW (Command Block Wrapper) — 主机发给设备的命令包, 31 字节 */
typedef struct _Bulk_Only_CBW {
    uint32_t dSignature;    /* 固定 0x43425355 ("USBC") */
    uint32_t dTag;          /* 主机生成, CSW 必须回填相同值 */
    uint32_t dDataLength;   /* 数据阶段总字节数 */
    uint8_t  bmFlags;       /* bit7=0: OUT 数据, bit7=1: IN 数据 */
    uint8_t  bLUN;          /* 目标 LUN 号 */
    uint8_t  bCBLength;     /* SCSI 命令块长度 (1~16) */
    uint8_t  CB[16];        /* SCSI 命令块 */
} Bulk_Only_CBW;

/* CSW (Command Status Wrapper) — 设备回给主机的状态包, 13 字节 */
typedef struct _Bulk_Only_CSW {
    uint32_t dSignature;    /* 固定 0x53425355 ("USBS") */
    uint32_t dTag;          /* 与 CBW.dTag 相同 */
    uint32_t dDataResidue;  /* 剩余未传输字节数 */
    uint8_t  bStatus;       /* 0=PASS, 1=FAIL, 2=PHASE_ERROR */
} Bulk_Only_CSW;
```

#### 3.1.2 状态机定义

```
                ┌──────────┐
                │ BOT_IDLE │ ← 等待 CBW
                └────┬─────┘
                     │ EP2 OUT 收到 31B
                     ▼
              ┌──────────────┐
              │ CBW_Decode() │ → 解析 SCSI 命令
              └──┬───────┬───┘
                 │       │
        命令需 IN 数据    命令需 OUT 数据
                 │       │
                 ▼       ▼
        ┌──────────┐  ┌───────────┐
        │BOT_DATA_IN│  │BOT_DATA_OUT│
        └────┬─────┘  └─────┬─────┘
             │              │
        最后一包        每包写完
             │              │
             ▼              ▼
   ┌─────────────────┐
   │BOT_DATA_IN_LAST │ → 发完最后一包 IN 数据
   └────────┬────────┘
            │
            ▼
   ┌──────────────┐
   │ BOT_CSW_Send  │ → EP1 IN 发送 CSW
   └──────┬───────┘
          │ EP1 IN 完成
          ▼
   ┌──────────┐
   │ BOT_IDLE │ → 重新使能 EP2 OUT, 等待下一个 CBW
   └──────────┘

异常路径: Bot_Abort() → STALL 端点 → BOT_ERROR → 等待 Bulk-Only Reset
```

| 状态 | 值 | 含义 |
|---|---|---|
| `BOT_IDLE` | 0 | 等待 CBW，EP2 OUT 接收就绪 |
| `BOT_DATA_OUT` | 1 | 正在接收 WRITE 数据 |
| `BOT_DATA_IN` | 2 | 正在发送 READ 数据（还有后续包） |
| `BOT_DATA_IN_LAST` | 3 | 最后一包 IN 数据已发，准备发 CSW |
| `BOT_CSW_Send` | 4 | 正在发送 CSW |
| `BOT_ERROR` | 5 | 出错，端点已 STALL，等待 Bulk-Only Reset |

#### 3.1.3 全局变量

```c
uint8_t  Bot_State;                           /* 当前状态 */
uint8_t  Bulk_Data_Buff[BULK_MAX_PACKET_SIZE]; /* 收发缓冲区 (64B) */
uint16_t Data_Len;                            /* 本次 EP2 OUT 收到的字节数 */
Bulk_Only_CBW CBW;                             /* 当前命令包 */
Bulk_Only_CSW CSW;                             /* 待发送的状态包 */
uint32_t SCSI_LBA;                             /* READ10/WRITE10 起始扇区号 */
uint32_t SCSI_BlkLen;                          /* READ10/WRITE10 扇区数 */
```

#### 3.1.4 函数实现

**`Mass_Storage_Out()` — EP2 OUT 中断入口**

```
Mass_Storage_Out()
├── Data_Len = USB_SIL_Read(EP2_OUT, Bulk_Data_Buff)   // 从 PMA 读出收到的数据
├── switch (Bot_State)
│   ├── BOT_IDLE:
│   │   └── CBW_Decode()                                // 首包: 解码 CBW
│   ├── BOT_DATA_OUT:
│   │   └── (TR2-S3 才实现 WRITE10 数据写入)
│   │       暂时走 Bot_Abort + Set_CSW(FAIL)
│   └── default:
│       └── Bot_Abort(BOTH_DIR) + Set_CSW(PHASE_ERROR)
```

**`Mass_Storage_In()` — EP1 IN 中断入口**

```
Mass_Storage_In()
├── switch (Bot_State)
│   ├── BOT_CSW_Send / BOT_ERROR:
│   │   ├── Bot_State = BOT_IDLE                    // CSW 已发完
│   │   └── SetEPRxStatus(ENDP2, EP_RX_VALID)       // 重新使能 EP2 OUT 等下一个 CBW
│   ├── BOT_DATA_IN:
│   │   └── (TR2-S3 才实现 READ10 继续发送)
│   │       暂时不触发
│   ├── BOT_DATA_IN_LAST:
│   │   └── Set_CSW(CSW_CMD_PASSED, SEND_CSW_ENABLE) // 数据发完, 发 CSW
│   └── default: break
```

**`CBW_Decode()` — CBW 解码与命令分发**

```
CBW_Decode()
├── 将 Bulk_Data_Buff 拷贝到 CBW 结构体
├── CSW.dTag = CBW.dTag                              // 回填 Tag
├── CSW.dDataResidue = CBW.dDataLength               // 初始化残留量
│
├── if (Data_Len != 31)
│   └── Bot_Abort(BOTH_DIR) + Set_CSW(FAIL)          // 包长度错误
│
├── if (CBW.CB[0] == READ10 || WRITE10)
│   └── 解析 SCSI_LBA 和 SCSI_BlkLen                  // 从 CB 字段提取
│
├── if (CBW.dSignature != 0x43425355)
│   └── Bot_Abort(BOTH_DIR) + Set_CSW(FAIL)          // 签名错误
│
├── if (bLUN > Max_Lun || bCBLength < 1 || bCBLength > 16)
│   └── Bot_Abort(BOTH_DIR) + Set_CSW(FAIL)          // 参数非法
│
└── switch (CBW.CB[0])  // SCSI 命令分发
    ├── TR2-S1: 所有命令 → Set_CSW(CSW_CMD_FAILED, SEND_CSW_ENABLE)
    │             (SCSI 处理在 TR2-S2/S3 实现)
    └── default: Bot_Abort + Set_CSW(FAIL)           // 未知命令
```

> **TR2-S1 阶段简化**：参考工程在 `CBW_Decode()` 的 switch 中列出了所有 SCSI 命令并调用对应处理函数。TR2-S1 阶段这些函数尚不存在，因此 switch 的每个 case 都先走 `Set_CSW(CSW_CMD_FAILED, SEND_CSW_ENABLE)` 路径。等 TR2-S2/S3 实现具体命令时再替换。

**`Transfer_Data_Request()` — 发送数据到主机**

```
Transfer_Data_Request(Data_Pointer, Data_Len)
├── USB_SIL_Write(EP1_IN, Data_Pointer, Data_Len)   // 数据写入 PMA
├── SetEPTxStatus(ENDP1, EP_TX_VALID)                // 触发 IN 传输
├── Bot_State = BOT_DATA_IN_LAST                      // 等待 IN 完成后发 CSW
├── CSW.dDataResidue -= Data_Len
└── CSW.bStatus = CSW_CMD_PASSED
```

**`Set_CSW()` — 发送 CSW**

```
Set_CSW(CSW_Status, Send_Permission)
├── CSW.dSignature = 0x53425355
├── CSW.bStatus = CSW_Status
├── USB_SIL_Write(EP1_IN, &CSW, 13)                  // CSW 写入 PMA
├── Bot_State = BOT_ERROR                             // 默认进入错误态
└── if (Send_Permission)
    ├── Bot_State = BOT_CSW_Send
    └── SetEPTxStatus(ENDP1, EP_TX_VALID)             // 触发 CSW 发送
```

**`Bot_Abort()` — STALL 端点**

```
Bot_Abort(Direction)
├── DIR_IN:  SetEPTxStatus(ENDP1, EP_TX_STALL)
├── DIR_OUT: SetEPRxStatus(ENDP2, EP_RX_STALL)
└── BOTH_DIR: 两者都 STALL
```

### 3.2 端点回调 (`usb_endp.c`)

新建 `src/usb_endp.c`，提供两个函数，供 `usb_istr.c` 的回调表通过宏引用：

```c
#include "usb_lib.h"
#include "usb_bot.h"
#include "usb_istr.h"

void EP1_IN_Callback(void)
{
    Mass_Storage_In();
}

void EP2_OUT_Callback(void)
{
    Mass_Storage_Out();
}
```

> 这两个函数名必须与 `usb_conf.h` 中的宏一致。TR1 阶段宏值是 `NOP_Process`，TR2-S1 改为引用这两个函数。

### 3.3 端点配置修改 (`usb_conf.h`)

```diff
- /* TR1 阶段 EP1/EP2 不需要真正工作，全部指向 NOP_Process */
- #define  EP1_IN_Callback   NOP_Process
+ /* TR2-S1: EP1 IN / EP2 OUT 接管为 BOT 状态机回调 (实现在 usb_endp.c) */
+ /* EP1_IN_Callback 和 EP2_OUT_Callback 由 usb_endp.c 提供函数实体, 此处不宏定义 */
```

其余端点（EP2_IN ~ EP7、EP1_OUT、EP3_OUT ~ EP7_OUT）仍保持 `NOP_Process`。

**关键点**：`usb_istr.c` 中 `pEpInt_IN[0]` 引用 `EP1_IN_Callback`，`pEpInt_OUT[1]` 引用 `EP2_OUT_Callback`。当 `usb_conf.h` 不再 `#define` 这两个宏时，编译器会在链接阶段找到 `usb_endp.c` 中定义的同名函数。这是 STM32 USB 库的标准机制——宏定义优先，没有宏时才用真实函数。

### 3.4 设备属性修改 (`usb_prop.c`)

#### 3.4.1 `MASS_Reset()` — 初始化 BOT 状态

在 `MASS_Reset()` 末尾（`bDeviceState = ATTACHED` 之后）追加：

```c
    /* TR2-S1: 初始化 BOT 状态机 */
    Bot_State = BOT_IDLE;
    CBW.dSignature = BOT_CBW_SIGNATURE;
```

#### 3.4.2 `MASS_NoData_Setup()` — Bulk-Only Reset 接入 BOT

TR1 阶段的 `MASS_NoData_Setup()` 在处理 `MASS_STORAGE_RESET` 时仅清 DTOG。TR2-S1 追加 BOT 状态重置：

```c
        ClearDTOG_TX(ENDP1);
        ClearDTOG_RX(ENDP2);

+       /* TR2-S1: 重置 BOT 状态机 */
+       Bot_State = BOT_IDLE;
+       CBW.dSignature = BOT_CBW_SIGNATURE;
```

#### 3.4.3 `MASS_Reset()` — SetConfiguration 追加 BOT 复位

`Mass_Storage_SetConfiguration()` 在配置成功时已清 DTOG，追加 BOT 重置：

```c
    if (pInformation->Current_Configuration != 0) {
        bDeviceState = CONFIGURED;
        ClearDTOG_TX(ENDP1);
        ClearDTOG_RX(ENDP2);
+       Bot_State = BOT_IDLE;
+       CBW.dSignature = BOT_CBW_SIGNATURE;
    }
```

### 3.5 头文件包含关系

```
usb_bot.h
  └── 被 usb_endp.c 引用 (调用 Mass_Storage_In/Out)
  └── 被 usb_prop.c 引用 (MASS_Reset 初始化 BOT 状态)

usb_endp.c
  └── #include "usb_bot.h"
  └── #include "usb_istr.h"  (pEpInt_IN/OUT 数组声明)

usb_prop.c
  └── #include "usb_bot.h"   (新增, 引用 Bot_State / CBW)
```

---

## 4. 数据流：一次 CBW → CSW 往返

```
时间线      PC (USBSTOR.SYS)                        STM32 侧
─────────────────────────────────────────────────────────────────
T1          发送 INQUIRY CBW (31B)
            → EP2 OUT                        EP2 OUT 中断
                                                   → pEpInt_OUT[1] = EP2_OUT_Callback
                                                   → Mass_Storage_Out()
                                                   → USB_SIL_Read(EP2_OUT, Bulk_Data_Buff)
                                                   → Data_Len = 31
                                                   → Bot_State == BOT_IDLE
                                                   → CBW_Decode()
                                                     → 拷贝到 CBW 结构体
                                                     → 签名校验通过
                                                     → CBW.CB[0] = 0x12 (INQUIRY)
                                                     → (TR2-S1) Set_CSW(FAIL, SEND_CSW)
                                                       → CSW 写入 PMA
                                                       → Bot_State = BOT_CSW_Send
                                                       → SetEPTxStatus(EP1, TX_VALID)
T2          发送 IN 令牌                        EP1 IN 中断
            ← 读取 CSW (13B)                       → pEpInt_IN[0] = EP1_IN_Callback
                                                   → Mass_Storage_In()
                                                   → Bot_State == BOT_CSW_Send
                                                   → Bot_State = BOT_IDLE
                                                   → SetEPRxStatus(EP2, EP_RX_VALID)
                                                   (等待下一个 CBW)
T3          收到 CSW (bStatus=0x01 FAIL)
            → USBSTOR 记录命令失败
            → 发送下一条 CBW...
```

> TR2-S1 阶段所有 SCSI 命令都会返回 `CSW_CMD_FAILED`，Windows 会反复重试不同命令。这是正常的——TR2-S2 实现 INQUIRY/READ_CAPACITY 后就会开始返回有效数据。

---

## 5. PMA 缓冲区使用

TR2-S1 不新增 PMA 缓冲区，EP1 TX 和 EP2 RX 的地址已在 TR1 配置好：

| 缓冲区 | PMA 地址 | 用途 |
|---|---|---|
| `ENDP1_TXADDR` (0x60) | EP1 IN | CSW (13B) / SCSI 响应数据 (TR2-S2+) |
| `ENDP2_RXADDR` (0x80) | EP2 OUT | CBW (31B) / WRITE 数据 (TR2-S3) |

64 字节足够容纳 CBW(31B) 和 CSW(13B)，无需扩展。

---

## 6. 文件清单：需新建 / 需修改

### 6.1 需新建的文件

| 文件 | 说明 |
|---|---|
| `src/usb_bot.c` | BOT 状态机实现 |
| `inc/usb_bot.h` | CBW/CSW 结构体、状态定义、函数声明 |
| `src/usb_endp.c` | EP1_IN_Callback / EP2_OUT_Callback |

### 6.2 需修改的文件

| 文件 | 修改内容 |
|---|---|
| `inc/usb_conf.h` | 删除 `EP1_IN_Callback` 和 `EP2_OUT_Callback` 的 `NOP_Process` 宏定义 |
| `src/usb_prop.c` | `MASS_Reset()` 追加 BOT 初始化；`Mass_Storage_SetConfiguration()` 追加 BOT 重置；`MASS_NoData_Setup()` 的 Bulk-Only Reset 追加 BOT 重置；新增 `#include "usb_bot.h"` |
| `project.uvprojx` | 添加 `usb_bot.c` 和 `usb_endp.c` 到编译列表 |

### 6.3 不变的文件

| 文件 | 说明 |
|---|---|
| `src/usb_istr.c` | 回调表引用的宏名不变，值变了但代码不需要改 |
| `src/usb_desc.c` | 描述符不变 |
| `src/hw_config.c` | 硬件配置不变 |
| `src/main.c` | 主程序不变 |

---

## 7. 验证方法

| 步骤 | 操作 | 预期结果 | 对应需求 |
|---|---|---|---|
| 1 | 编译烧录固件 | Keil 0 Error 0 Warning | — |
| 2 | USB 插入 PC | 设备管理器无黄色感叹号 | TR2-S1 ✅ |
| 3 | USBTreeView 查看端点 | EP1 IN + EP2 OUT 各 1 pipe，状态正常 | TR2-S1 ✅ |
| 4 | 用 USB 分析仪 / BusHound 抓包 | 能看到 CBW(31B) 和 CSW(13B) 往返 | TR2-S1 ✅ |
| 5 | 观察 CSW.bStatus | 所有命令返回 0x01 (CMD_FAILED)，因为 SCSI 命令尚未实现 | TR2-S1 ✅ (预期行为) |
| 6 | Windows 尝试访问磁盘 | 提示"请插入磁盘"或"无法访问" | 正常，TR2-S2 实现查询命令后即可访问 |

> **TR2-S1 最小验收**：步骤 1~3 通过。设备无感叹号 + 2 个 pipe 正常 = BOT 骨架已通。

---

## 8. 常见问题排查

| 现象 | 可能原因 | 排查方向 |
|---|---|---|
| 设备出现黄色感叹号 | `usb_conf.h` 未删除 EP1_IN/EP2_OUT 的 NOP 宏 | 确认宏已删除，`usb_endp.c` 已加入编译 |
| | `usb_endp.c` 未加入工程编译 | 检查 `project.uvprojx` 文件列表 |
| HardFault | `CBW_Decode` 访问 `Bulk_Data_Buff` 越界 | 确认 `BULK_MAX_PACKET_SIZE >= 64` |
| | `usb_bot.c` 未包含 `usb_bot.h` | 检查 include |
| 收不到 CBW | EP2 OUT 状态不是 `EP_RX_VALID` | 检查 `MASS_Reset()` 是否设置了 `SetEPRxStatus(ENDP2, EP_RX_VALID)` |
| | `Bot_State` 未初始化为 `BOT_IDLE` | 检查 `MASS_Reset()` 末尾 |
| CSW 发不出 | EP1 TX 状态不是 `EP_TX_VALID` | 检查 `Set_CSW()` 是否调用了 `SetEPTxStatus(ENDP1, EP_TX_VALID)` |
| CSW bStatus 一直是 0x02 | PHASE_ERROR，状态机走错 | 用调试器观察 `Bot_State` 流转 |
| 主机一直重试命令 | 所有命令返回 FAIL 是 TR2-S1 预期行为 | 非错误，TR2-S2 实现 SCSI 命令后改善 |

---

## 9. 与其他需求的关系

| 需求 | 关系 |
|---|---|
| TR1-01 ~ TR1-06 | **前置**。TR1 枚举通过后 BOT 状态机才有意义 |
| TR1-05 (软件重连) | 无直接关系，但重连后需重新初始化 BOT 状态 |
| TR2-S2 (SCSI 查询命令) | **后继**。TR2-S1 的 `CBW_Decode()` switch 为 TR2-S2 预留了命令分发入口 |
| TR2-S3 (SCSI 读写命令) | **后继**。TR2-S1 的 `BOT_DATA_IN` / `BOT_DATA_OUT` 状态为 TR2-S3 预留了数据传输入口 |
| TR2-S4 (安全弹出) | **后继**。`START_STOP_UNIT` 命令需走 `CBW_Decode()` 分发 |

---

*文档结束。*
