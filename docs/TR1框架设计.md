# TR1 框架设计 — USB 枚举通过

## 文档信息

| 项目 | 内容 |
|---|---|
| 项目名称 | stm32-w25q32-msd |
| 文档类型 | TR1 阶段框架设计 |
| 版本 | V1.0 |
| 日期 | 2026-07-17 |
| 前置文档 | `项目框架设计.md`（整体分层与组件定义） |

---

## 1. 设计目标

让 PC 设备管理器识别到 STM32 模拟的 USB 大容量存储设备，并完整走通 USB 枚举流程（设备/配置/字符串描述符、标准请求、MSC 类请求、软件重连）。

**与 `项目框架设计.md` 的关系**：本文档是整体框架中 §6.1 TR1 的细化展开，把"USB 枚举通过"这一个粗粒度目标，拆成三个子阶段、每条需求对应一次可独立提交的代码改动。

---

## 2. 拆分原则

### 2.1 为什么不能直接按"主机可见现象"拆分

USB 枚举是主机发起的连续流程：`RESET → GET_DESCRIPTOR(Device) → SET_ADDRESS → GET_DESCRIPTOR(Config) → GET_DESCRIPTOR(String) → SET_CONFIGURATION`。代码侧任何一个组件缺失都会让整个流程失败，所以"设备管理器出现设备""描述符正确""字符串正确"这些现象级需求在物理提交上是耦合的——一次代码改动会同时满足多条。

### 2.2 本文档的拆分方式

采用**骨架先行 + 纵向切片 + 横向补全**三步：

| 子阶段 | 比喻 | 核心特征 |
|---|---|---|
| **TR1-A 骨架** | 打地基 | 协议栈代码全部到位、能编译、能进入 USB 中断，但 PC 尚未识别设备 |
| **TR1-B 最小枚举** | 立框架 | 一次最小改动让设备出现在设备管理器（即使带感叹号） |
| **TR1-C 逐层补全** | 装修 | 每条需求只改一个文件的一小块，每次插 USB 都有可观测差异 |

**关键约束**：每条需求对应一次 git 提交，提交的 diff 范围明确（只动一个组件的一小块），且提交后有确定的可观测结果。

---

## 3. 子阶段定义

### 3.1 TR1-A — USB 栈骨架可编译运行

**目标**：让 USB 协议栈代码全部到位、能编译、能烧录、能进入 USB 中断。**不要求 PC 识别设备**。

这是学习者理解 USB 外设物理基础的关键阶段：USB 外设如何被点亮、中断从哪来、PMA 是什么。

| 编号 | 内容 | 可观测结果 | 涉及组件 |
|---|---|---|---|
| TR1-A1 | 工程骨架 + 标准外设库 + USB 库集成，`main.c` 空壳可编译 | Keil 编译通过、链接通过 | Library, Start, project.uvprojx |
| TR1-A2 | `hw_config.c/h` 硬件初始化（HCLK 72MHz / USB 48MHz / PA11 PA12 / NVIC） | 断点能跑到 `while(1)`，PA12 已切为 AF_PP | hwinit |
| TR1-A3 | `usb_conf.h` 端点数 / PMA 缓冲区地址 / 回调全指向 `NOP_Process` | 编译通过，PMA 布局固定 | endpoint |
| TR1-A4 | `usb_istr.c/h` USB 中断入口 `USB_LP_CAN1_RX0_IRQHandler` + `USB_Istr()` ISTR 事件分发 | 插 USB 后断点能命中 `USB_Istr()`，ISTR 的 RESET 位被置位 | isr |

**阶段产物**：可烧录固件，插入 USB 后用示波器能看到 PA12 上的 D+ 上拉，PC 端可能短暂出现"未知设备"后消失（因为没有响应 GET_DESCRIPTOR，属正常）。

---

### 3.2 TR1-B — 最小枚举通过（纵向切片）

**目标**：一个最小可工作的枚举——只实现 PC 能看到的最低限度。对应 USB 规范 §9.1.1 的 `Attached → Powered → Default` 状态。

| 编号 | 内容 | 可观测结果 | 涉及组件 |
|---|---|---|---|
| TR1-B1 | `usb_desc.c/h` 最小设备描述符（18B，VID=0x0483 PID=0x5720 bcdUSB=2.00 bMaxPacketSize0=64） | 主机 GET_DESCRIPTOR(Device) 能拿到 18B 合法数据 | desc |
| TR1-B2 | `usb_prop.c/h` MASS_Reset + 设备属性表 `DEVICE_PROP` + `MASS_GetDeviceDescriptor` 回调 + 标准请求表 `Device_Table` | PC 设备管理器出现 "Unknown Device" 或带黄色感叹号的设备（**TR1-01 的真正含义**） | device |
| TR1-B3 | `usb_pwr.c/h` PowerOn + `bDeviceState` 状态机（UNCONNECTED→ATTACHED→POWERED→DEFAULT） + `fSuspendEnabled=FALSE` | D+ 上拉生效，主机发起 RESET，设备进入 DEFAULT 状态 | power |
| TR1-B4 | `main.c` 主流程串联（Set_System → Set_USBClock → USB_Interrupts_Config → USB_Init → PowerOn → while） | 烧录后流程跑通，设备出现在设备管理器 | main |

**阶段产物**：插入 USB 后设备管理器出现设备（可能带黄色感叹号，因为没有配置描述符和字符串）。这就是真正的"TR1-01 可被主机检测到"。

> **关键**：TR1-B2 完成时设备管理器就会出现设备。后续 TR1-C 每一条都是"在某一个已有接口里填实现"，不再需要大改动。

---

### 3.3 TR1-C — 枚举完整化（横向补全）

**目标**：补齐配置/字符串描述符、标准请求响应、类请求响应、软件重连。**每条需求都是一次独立提交**，且每次提交后插 USB 都能立即看到差异。

| 编号 | 内容 | 可观测结果 | 对应原需求 |
|---|---|---|---|
| TR1-C1 | 配置描述符（9B 配置 + 9B 接口 + 7B EP1 + 7B EP2，MSC/Bulk-Only/自供电） | USBTreeView 显示 Mass Storage Class, 2 Bulk EP | 原 TR1-03 |
| TR1-C2 | 字符串描述符（厂商/产品/序列号，序列号读 MCU UID 生成 12 位十六进制） | USBTreeView 显示 "STMicroelectronics" / "STM32 W25Q32 Flash Disk" / 唯一 SN | 原 TR1-04 |
| TR1-C3 | SET_ADDRESS / SET_CONFIGURATION 完整响应（标准请求回调 `Device_Table` 完整实现） | 枚举全程无 STALL，`bDeviceState = CONFIGURED` | 原 TR1-06 |
| TR1-C4 | MSC 类请求 GET_MAX_LUN（返回 1 字节） / Bulk-Only Mass Storage Reset（清 DTOG） | 设备管理器黄色感叹号消失（USBSTOR 驱动可加载） | v0.2.2 修复项 |
| TR1-C5 | 软件重连 `USB_Cable_Config(DISABLE/ENABLE)`（PA12 Out_PP ↔ AF_PP 切换） | 调用函数后 PC 重新枚举 | 原 TR1-05 |

**阶段产物**：设备管理器无黄色感叹号，USBTreeView 可查看完整描述符，软件重连可用。TR1 阶段全部完成。

> **关键改进**：TR1-C1/C2/C3 是纯增量改动，每次只动 `usb_desc.c` 或 `usb_prop.c` 的一小块，且每次都有 USBTreeView 可观测差异。TR1-C4 单独列出——黄色感叹号修复是一个独立的协议层 bug，不应和"枚举通过"绑在一起。

---

## 4. 与原需求列表的映射

原 `需求列表.md` 的 TR1-01~06 描述的是"最终状态该满足什么"，作为**验收清单**仍有价值。本文档的 TR1-A/B/C 描述的是**开发任务**，按 A→B→C 顺序提交，最终用原 TR1-01~06 检查。

| 开发任务 | 对应验收需求 | 关系 |
|---|---|---|
| TR1-A1~A4 | 无（骨架不要求 PC 识别） | 前置基础 |
| TR1-B1~B4 | TR1-01（设备管理器出现设备）、TR1-02（设备描述符正确） | B2 完成即满足 TR1-01，B1 完成即满足 TR1-02 |
| TR1-C1 | TR1-03（配置描述符正确） | 直接对应 |
| TR1-C2 | TR1-04（字符串描述符正确） | 直接对应 |
| TR1-C3 | TR1-06（可响应 USB 标准请求） | 直接对应 |
| TR1-C4 | 无原需求（v0.2.2 修复项） | 补充项 |
| TR1-C5 | TR1-05（支持软件重连） | 直接对应 |

---

## 5. 组件清单（TR1 阶段产出文件）

### 5.1 TR1 阶段涉及的全部文件

| 文件 | 所属组件 | 所属子阶段 | 说明 |
|---|---|---|---|
| `src/main.c` | main | TR1-B4 | 主流程串联 |
| `src/hw_config.c` | hwinit | TR1-A2 | 时钟/GPIO/NVIC 初始化 |
| `inc/hw_config.h` | hwinit | TR1-A2 | 接口声明 |
| `inc/usb_conf.h` | endpoint | TR1-A3 | 端点/PMA 地址/回调宏 |
| `src/usb_istr.c` | isr | TR1-A4 | USB 中断服务 + ISTR 分发 |
| `inc/usb_istr.h` | isr | TR1-A4 | 接口声明 |
| `src/usb_desc.c` | desc | TR1-B1 / TR1-C1~C2 | 描述符数据 |
| `inc/usb_desc.h` | desc | TR1-B1 | 描述符声明 |
| `src/usb_prop.c` | device | TR1-B2 / TR1-C3~C4 | 设备属性 + 标准请求 + 类请求 |
| `inc/usb_prop.h` | device | TR1-B2 | 接口声明 |
| `src/usb_pwr.c` | power | TR1-B3 / TR1-C5 | 电源管理 + 状态机 |
| `inc/usb_pwr.h` | power | TR1-B3 | 接口声明 |

### 5.2 TR1 阶段不涉及的文件

`bot.c/h`、`scsi.c/h`、`buffer.c/h`、`mal.c/h`、`flash.c/h` —— TR1 不实现任何存储逻辑。EP1(IN)/EP2(OUT) 的端点回调全部指向 `NOP_Process`（见 `usb_conf.h`），仅占位以通过编译。

---

## 6. 关键技术决策

### 6.1 D+ 上拉方案

本板无 PMOS 开关，R10 硬接到 3.3V（1.5kΩ 固定上拉）。软件重连通过 PA12 在 `GPIO_Mode_Out_PP`（输出低拉低 D+）↔ `GPIO_Mode_AF_PP`（交由 USB 外设）之间切换实现。

- `Set_System()` 阶段：PA12 先设为 GPIO 输出低（让主机认为设备未连接）
- `PowerOn()` 阶段：PA12 切为 AF_PP（通知主机有设备插入）

### 6.2 禁用挂起模式

`fSuspendEnabled = FALSE`（`usb_pwr.c`），USBWakeUp 中断 `DISABLE`（`hw_config.c`）。

主机在发 RESET 前的总线空闲期会触发 SUSP 中断，若 `fSuspendEnabled` 为 `TRUE` 则 MCU 进入 STOP 模式且无法唤醒（USBWakeUp 中断未配置），导致枚举彻底失败。这是 TR1 阶段的关键约束。

### 6.3 PMA 缓冲区布局

512 字节 PMA（0x40006000 ~ 0x400061FF），按 16-bit 字偏寻址（库函数内部 ×2 转字节地址）：

```
0x00   BTABLE         32 字 = 64B   (端点 0~7 的 Buffer Description Table)
0x20   ENDP0_RXADDR   32 字 = 64B   (EP0 接收)
0x40   ENDP0_TXADDR   32 字 = 64B   (EP0 发送)
0x60   ENDP1_TXADDR   32 字 = 64B   (EP1 Bulk IN)
0x80   ENDP2_RXADDR   32 字 = 64B   (EP2 Bulk OUT)
0xA0   (96 字 = 192 字节空闲, 到 PMA 末尾 0xFF)
```

> **历史教训**：v0.2.1 曾因 PMA 地址写成字节偏移导致与 BTABLE 重叠，枚举失败。必须使用 16-bit 字偏移。

### 6.4 USB 时钟来源

HCLK = 72MHz（HSE 8MHz × 9 PLL），USB 时钟 = PLLCLK ÷ 1.5 = 48MHz ± 0.25%。**必须精确**，否则枚举不稳定。

### 6.5 中断配置

| 中断 | 优先级组 | 抢占优先级 | 响应优先级 |
|---|---|---|---|
| USB_LP_CAN1_RX0_IRQn | 2 | 0 | 0 |
| USBWakeUp_IRQn | — | — | DISABLE（TR1 不使用） |

### 6.6 端点分配

| 端点 | 方向 | 类型 | 包大小 | 用途 | TR1 阶段回调 |
|---|---|---|---|---|---|
| EP0 | IN/OUT | Control | 64B | 枚举 / 标准请求 | 核心库内置 |
| EP1 | IN | Bulk | 64B | 数据读取 + CSW | `NOP_Process`（TR2 替换） |
| EP2 | OUT | Bulk | 64B | 数据写入 + CBW | `NOP_Process`（TR2 替换） |

### 6.7 设备标识

| 字段 | 值 |
|---|---|
| idVendor | 0x0483 (STMicroelectronics) |
| idProduct | 0x5720 |
| bcdUSB | 0x0200 (USB 2.0) |
| bMaxPacketSize0 | 64 |
| bDeviceClass | 0x00 (类在接口级定义) |
| bNumConfigurations | 1 |
| iManufacturer / iProduct / iSerialNumber | 1 / 2 / 3 |

---

## 7. 提交节奏建议

每个子阶段对应一个 git 提交，提交信息建议格式：

```
TR1-A1: 集成标准外设库与USB库, main.c 空壳可编译
TR1-A2: hw_config 硬件初始化 (时钟/GPIO/NVIC)
TR1-A3: usb_conf.h 端点与PMA缓冲区配置
TR1-A4: usb_istr USB中断服务与ISTR事件分发
TR1-B1: usb_desc 最小设备描述符 (18B)
TR1-B2: usb_prop 设备属性表与MASS_Reset, 设备管理器出现设备
TR1-B3: usb_pwr PowerOn与状态机
TR1-B4: main 主流程串联, 设备出现在设备管理器
TR1-C1: 配置描述符 (MSC/Bulk-Only/2 Endpoint)
TR1-C2: 字符串描述符 (厂商/产品/序列号读UID)
TR1-C3: SET_ADDRESS/SET_CONFIGURATION完整响应
TR1-C4: MSC类请求 GET_MAX_LUN/Bulk-Only Reset
TR1-C5: 软件重连 USB_Cable_Config
```

共 13 次提交，每次 diff 范围明确，可独立 review。

---

## 8. 验证流程

### 8.1 TR1-A 验证（骨架）

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | Keil 编译 | 0 Error, 0 Warning | TR1-A1 ✅ |
| 2 | 烧录，断点在 `main()` 入口 | 命中断点 | TR1-A2 ✅ |
| 3 | 单步到 `while(1)` | PA12 已切为 AF_PP（示波器/万用表可测） | TR1-A2 ✅ |
| 4 | 断点设在 `USB_Istr()`，插 USB | 命中断点，ISTR.RESET 被置位 | TR1-A4 ✅ |

### 8.2 TR1-B 验证（最小枚举）

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 烧录，USB 插入 PC | Windows 发出设备插入提示音 | — |
| 2 | 设备管理器 → 通用串行总线控制器 | 出现 "Unknown Device" 或带感叹号设备 | TR1-B2 ✅ / 原 TR1-01 ✅ |
| 3 | USBTreeView 查看设备描述符 | VID=0x0483 PID=0x5720 bcdUSB=2.00 bMaxPacketSize0=64 | TR1-B1 ✅ / 原 TR1-02 ✅ |

### 8.3 TR1-C 验证（逐层补全）

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | USBTreeView 查看配置描述符 | Mass Storage Class, Bulk-Only, 2 Bulk EP, 自供电 | TR1-C1 ✅ / 原 TR1-03 ✅ |
| 2 | USBTreeView 查看字符串描述符 | "STMicroelectronics" / "STM32 W25Q32 Flash Disk" / 唯一 SN | TR1-C2 ✅ / 原 TR1-04 ✅ |
| 3 | 设备管理器查看设备状态 | 无黄色感叹号，`bDeviceState=CONFIGURED` | TR1-C3+C4 ✅ / 原 TR1-06 ✅ |
| 4 | 拔插 USB 或调用 `USB_Cable_Config` | 设备重新枚举 | TR1-C5 ✅ / 原 TR1-05 ✅ |

---

## 9. 与 TR2 的衔接

TR1 完成后，EP1/EP2 的回调仍指向 `NOP_Process`。TR2-S1 将接管这两个回调，把 `NOP_Process` 替换为真实 BOT 状态机，开始走通 SCSI 命令。TR1 的所有组件保持不变，TR2 只新增 `bot/scsi/buffer/mal` 四个组件。

---

*文档结束。*
