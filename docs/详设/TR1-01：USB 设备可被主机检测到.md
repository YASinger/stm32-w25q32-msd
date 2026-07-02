# TR1-01 详细设计：USB 设备可被主机检测到

## 修改记录

| 版本 | 日期 | 修改内容 | 修改人 |
|---|---|---|---|
| V1.0 | 2026-06-09 | 初始版本，基于 PMOS 控制 D+ 上拉的假设 | Copilot |
| V1.1 | 2026-06-09 | D+ 上拉方案更正为 PA12 GPIO 模式切换（本板无 PMOS，R10 硬接到 3.3V）；`USB_Cable_Config` 实现改用 PA12 AF_PP ↔ Out_PP 切换；删除 `USB_DISCONNECT` 引脚宏 | Copilot |
| V1.2 | 2026-06-10 | USB 中断入口 `USB_LP_CAN1_RX0_IRQHandler` 从 `stm32f10x_it.c` 移至 `usb_istr.c`（保持标准模板文件不被污染）；`USB_Istr()` 由核心库说更正为应用层实现 | Copilot |
| V1.3 | 2026-07-03 | 对应代码 v0.2.0：正文同步至实际实现（`main.c` 显式调用 `PowerOn()`；新增 PC13 LED 枚举成功指示；`USBWakeUp_IRQn` 由 `ENABLE` 改为 `DISABLE`） | Copilot |
| V1.4 | 2026-07-03 | 对应代码 v0.2.1：修正 PMA 端点缓冲区地址为 16-bit 字偏移（ENDP0_RXADDR 0x18→0x20 等，原字节偏移与 BTABLE 重叠）；`fSuspendEnabled` 默认值由 `TRUE` 改为 `FALSE`，避免 SUSP 中断进入不可唤醒的 STOP 模式 | Copilot |

---

## 文档信息

| 项目 | 内容 |
|---|---|
| 需求编号 | TR1-01 |
| 需求描述 | USB 设备可被主机检测到 |
| 验收标准 | 插入 USB 后 PC 设备管理器出现新设备 |
| 所属阶段 | TR1 — USB 枚举通过 |
| 文档版本 | V1.4 |
| 日期 | 2026-07-03 |

---

## 1. 需求分解

"PC 能检测到 USB 设备" 是 USB 枚举的最低门槛。从硬件上电到 PC 设备管理器出现新设备，需要完成以下子步骤：

```
MCU 上电 → 时钟初始化 → USB 48MHz 时钟就绪
         → GPIO 初始化 (PA11 DM, PA12 DP)
         → D+ 硬件上拉已存在 (1.5kΩ 固定接 VCC_3V3)
         → PA12 从 GPIO 输出低切换为 AF_PP (通知主机有设备插入)
         → NVIC 使能 USB 中断
         → USB 核心库初始化 (CNTR/ISTR/BTABLE)
         → PC 检测到 D+ 拉高, 发起 RESET
         → 设备响应 RESET, 端点 0 就绪
         → PC 发送 GET_DESCRIPTOR(Device)
         → 设备返回设备描述符
         → PC 识别为新设备, 出现在设备管理器
```

> **TR1-01 的边界**：只要设备管理器出现设备即可，不要求描述符完全正确（那是 TR1-02~04），不要求完成 SET_CONFIGURATION（那是 TR1-06）。但实践中设备描述符必须合法、端点 0 必须能响应，否则枚举会失败。

---

## 2. 涉及组件

按 DESIGN.md 的分层框架，TR1-01 涉及以下组件：

```
┌─────────────────────────────────────────────────┐
│                    PC (设备管理器)                │
│   目标: 出现 "STM32 W25Q32 Flash Disk"          │
└────────────────────┬────────────────────────────┘
                     │ USB FS (DP=PA12, DM=PA11)
╔════════════════════╧═════════════════════════════╗
║  main 主程序                                       ║
║  hw_config 硬件初始化 (USB 时钟/GPIO/NVIC/D+上拉)   ║
║  usb_conf 端点缓冲区配置                            ║
║  usb_desc USB 描述符                                ║
║  usb_prop 设备属性 + 标准请求                        ║
║  usb_istr USB 中断服务                              ║
║  usb_pwr 电源管理 + 状态机                          ║
║  [USB 核心库 + 标准外设库]                            ║
╚═══════════════════════════════════════════════════╝
```

| 组件 | 职责 | TR1-01 中的角色 |
|---|---|---|
| **hw_config** | 硬件平台初始化 | 时钟树、USB 48MHz、GPIO、D+ 上拉、NVIC 中断 |
| **usb_conf** | USB 端点资源配置 | 端点数量、缓冲区地址分配 |
| **usb_desc** | USB 描述符数据 | 提供设备描述符给 PC（至少是合法的 18 字节） |
| **usb_prop** | 设备属性回调表 | 端点初始化、Reset 处理、Setup 包处理、描述符 getter |
| **usb_istr** | USB 中断服务入口 | ISR → ISTR 事件分发 → 端点回调 |
| **usb_pwr** | 电源状态管理 | bDeviceState 状态机 (UNCONNECTED→ATTACHED→...→CONFIGURED) |
| **main** | 主程序入口 | 按序调用初始化，等待枚举完成 |

---

## 3. 各模块详细设计

### 3.1 硬件初始化 (`hw_config`)

#### 3.1.1 关键约束

| 约束 | 值 | 说明 |
|---|---|---|
| 系统时钟 | HCLK = 72MHz, PCLK1 = 36MHz, PCLK2 = 72MHz | `SystemInit()` 默认配置 (HSE 8MHz × 9 PLL) |
| USB 时钟 | 48MHz ± 0.25% | **必须精确**，来源 PLLCLK ÷ 1.5 = 72÷1.5 = 48MHz |
| USB DP/DM | PA12 (DP), PA11 (DM) | 复用推挽输出 (`GPIO_Mode_AF_PP`) |
| D+ 上拉 | 1.5kΩ 固定上拉到 3.3V (R10)，无 PMOS 开关 | 硬件硬接；软件重连通过 PA12 临时切为 GPIO 输出低实现 |
| USB 中断 | USB_LP_CAN1_RX0_IRQn | 优先级组 2，抢占优先级 0，响应优先级 0 |

#### 3.1.2 接口定义

```c
/* 函数声明 — 定义在 hw_config.h */
void Set_System(void);              // 系统时钟 + USB GPIO (PA11/PA12)
void Set_USBClock(void);            // USB 48MHz 时钟配置
void USB_Interrupts_Config(void);   // NVIC 中断配置
void USB_Cable_Config(FunctionalState NewState);  // 软件断开/重连 (通过 PA12 GPIO 模式切换)
void Get_SerialNum(void);           // 读取 MCU UID 生成序列号字符串
void MAL_Config(void);              // 存储介质初始化 (TR1 可为空)

/* 宏定义 — 定义在 platform_config.h */
#define BULK_MAX_PACKET_SIZE        0x00000040      // 64 字节
```

#### 3.1.3 `Set_System()` 处理流程

```
Set_System()
├── RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE)    // ① GPIOA 时钟
├── // ② 先将 PA12 设为 GPIO 输出低 → D+ 通过 22Ω 被拉低 → 主机认为设备断开
├── GPIO_Init(PA12, Out_PP, 50MHz)
├── GPIO_ResetBits(GPIOA, GPIO_Pin_12)
├── GPIO_Init(PA11, AF_PP, 50MHz)                            // ③ DM 引脚初始化
├── // ④ PA12 暂不切为 AF_PP，留给 PowerOn() 中 USB_Cable_Config(ENABLE) 处理
└── MAL_Config()                                             // ⑤ TR1 可为空
```

#### 3.1.4 `Set_USBClock()` 处理流程

```
Set_USBClock()
├── RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_1Div5)   // PLLCLK÷1.5 = 72÷1.5 = 48MHz
└── RCC_APB1PeriphClockCmd(RCC_APB1Periph_USB, ENABLE) // 使能 USB 外设时钟
```

> **为什么是 `1Div5`？** STM32F10x 参考手册：`RCC_USBCLKSource_PLLCLK_1Div5` 实际含义是 PLLCLK ÷ 1.5。宏命名容易误导，但实际分频比为 1.5。当 PLL 输出 72MHz 时，72 ÷ 1.5 = 48MHz，恰好满足 USB 全速时钟要求。

#### 3.1.5 `USB_Cable_Config()` 处理流程

```
USB_Cable_Config(ENABLE)  // "连接"
├── GPIO_Init(PA12, AF_PP, 50MHz)   // 恢复 USB 复用功能 → D+ 被 1.5kΩ 上拉到 3.3V → 主机检测到插入

USB_Cable_Config(DISABLE) // "断开"
├── GPIO_Init(PA12, Out_PP, 50MHz)  // 切换为 GPIO 输出
└── GPIO_ResetBits(GPIOA, GPIO_Pin_12)  // 输出低 → D+ 通过 22Ω 拉到 GND → 主机认为设备拔出
```

> **硬件原理**：本板的 D+ 上拉是硬接的——R10 (1.5kΩ) 一端接 VCC_3V3，另一端通过 R6 (22Ω 串阻) 连到 PA12。
> 没有 PMOS 开关可控制上拉通断。
> 软件重连利用 PA12 的 GPIO 模式切换：
> - **断开**：PA12 设为推挽输出低电平，D+ 通过 22Ω+MOS 下拉到 GND，主机端 D+ 电压被拉到 <0.8V，判定为断开。
> - **连接**：PA12 恢复为 AF_PP，USB PHY 接管，D+ 被 R10 上拉到 3.3V，主机检测到全速设备插入。
> ⚠️ 此方案的 22Ω 限流保护了 GPIO 和上拉电阻之间的冲突。
> 以下代码实现版本：
>
> ```c
> void USB_Cable_Config(FunctionalState NewState)
> {
>     GPIO_InitTypeDef GPIO_InitStructure;
>     GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_12;
>     GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
>     if (NewState == DISABLE) {
>         GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
>         GPIO_Init(GPIOA, &GPIO_InitStructure);
>         GPIO_ResetBits(GPIOA, GPIO_Pin_12);
>     } else {
>         GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
>         GPIO_Init(GPIOA, &GPIO_InitStructure);
>     }
> }
> ```

#### 3.1.6 `USB_Interrupts_Config()` 处理流程

```
USB_Interrupts_Config()
├── NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2)    // 2 位抢占 + 2 位响应
├── NVIC_Init(USB_LP_CAN1_RX0_IRQn, Preempt=2, Sub=0) // USB 低优先级中断
└── NVIC_Init(USBWakeUp_IRQn, Preempt=0, DISABLE)     // USB 唤醒中断 (TR1 禁用; fSuspendEnabled=FALSE 故不会触发)
```

### 3.2 USB 端点配置 (`usb_conf`)

#### 3.2.1 端点分配

| 端点 | 方向 | 类型 | 包大小 | 缓冲区地址 (字偏移) | 用途 |
|---|---|---|---|---|---|
| EP0 | IN/OUT | Control | 64B | RX=0x20, TX=0x40 | 枚举 / 标准请求 |
| EP1 | IN | Bulk | 64B | TX=0x60 | 数据读取 + CSW (TR2+) |
| EP2 | OUT | Bulk | 64B | RX=0x80 | 数据写入 + CBW (TR2+) |

> **PMA 缓冲区布局**（共 512 字节 = 256 字，地址 0x40006000~0x400061FF）：
> 注意：以下地址均为 16-bit 字偏移，USB 库函数内部会 ×2 转为字节地址。
> ```
> BTABLE (0x00):  32 字 = 64B (端点0~7 的 Buffer Description Table)
> EP0_RX (0x20):  32 字 = 64B  ─┐
> EP0_TX (0x40):  32 字 = 64B   ├─ 共 64×3=192B
> EP1_TX (0x60):  32 字 = 64B   │  不重叠，64B 对齐
> EP2_RX (0x80):  32 字 = 64B  ─┘
> 空闲: 0xA0~0xFF = 96 字 = 192B
> ```

#### 3.2.2 宏定义

```c
#define EP_NUM              (3)         // 使用 3 个端点 (EP0, EP1, EP2)

/* 缓冲区地址 (16-bit 字偏移, 库函数内部 ×2 转字节地址) */
#define BTABLE_ADDRESS      (0x00)      // 缓冲区描述表基址
#define ENDP0_RXADDR        (0x20)      // EP0 接收缓冲区
#define ENDP0_TXADDR        (0x40)      // EP0 发送缓冲区
#define ENDP1_TXADDR        (0x60)      // EP1 IN  发送缓冲区
#define ENDP2_RXADDR        (0x80)      // EP2 OUT 接收缓冲区

/* ISTR 事件掩码 */
#define IMR_MSK (CNTR_CTRM  | CNTR_WKUPM | CNTR_SUSPM | CNTR_ERRM  | CNTR_SOFM \
                 | CNTR_ESOFM | CNTR_RESETM)

/* 端点回调声明 (CTR_LP 使用) */
#define EP1_IN_Callback   NOP_Process     // TR1 阶段用空回调 (TR2 时替换)
#define EP2_IN_Callback   NOP_Process
#define EP3_IN_Callback   NOP_Process
// ... 其余均为 NOP_Process
#define EP1_OUT_Callback  NOP_Process
#define EP2_OUT_Callback  NOP_Process     // TR1 阶段用空回调 (TR2 时替换)
// ... 其余均为 NOP_Process
```

> **TR1 注意**：`EP1_IN_Callback` 和 `EP2_OUT_Callback` 在 TR1 阶段指向 `NOP_Process` 是可以的——因为 TR1 只需要枚举，不需要真正的数据传输。但 `usb_istr.c` 中的函数指针数组 `pEpInt_IN[0]` 和 `pEpInt_OUT[1]` 必须指向同名函数，所以 `usb_conf.h` 中的宏必须与 `usb_istr.c` 一致。

### 3.3 USB 描述符 (`usb_desc`)

#### 3.3.1 设备描述符 (18 字节)

TR1-01 只需要 18 字节的设备描述符合法即可让 PC 检测到设备：

| 偏移 | 字段 | 值 | 说明 |
|---|---|---|---|
| 0 | bLength | 0x12 (18) | 描述符长度 |
| 1 | bDescriptorType | 0x01 | 设备描述符类型 |
| 2~3 | bcdUSB | 0x0200 | USB 2.0 |
| 4 | bDeviceClass | 0x00 | 接口级别定义类 |
| 5 | bDeviceSubClass | 0x00 | |
| 6 | bDeviceProtocol | 0x00 | |
| 7 | bMaxPacketSize0 | 0x40 (64) | EP0 最大包大小 |
| 8~9 | idVendor | 0x0483 | STMicroelectronics |
| 10~11 | idProduct | 0x5720 | |
| 12~13 | bcdDevice | 0x0200 | 设备版本 2.00 |
| 14 | iManufacturer | 1 | 字符串索引 1 |
| 15 | iProduct | 2 | 字符串索引 2 |
| 16 | iSerialNumber | 3 | 字符串索引 3 |
| 17 | bNumConfigurations | 1 | 1 个配置 |

#### 3.3.2 配置描述符 (32 字节)

虽然 TR1-01 不要求 PC 完成 SET_CONFIGURATION，但配置描述符仍需要在 GET_DESCRIPTOR(Config) 请求时返回。一个合法的最小 MSC 配置描述符：

```
配置描述符 (9B):
  bLength=9, bDescriptorType=2 (CONFIGURATION)
  wTotalLength=32, bNumInterfaces=1
  bConfigurationValue=1, iConfiguration=0
  bmAttributes=0xC0 (自供电), bMaxPower=50 (100mA)

接口描述符 (9B):
  bLength=9, bDescriptorType=4 (INTERFACE)
  bInterfaceNumber=0, bAlternateSetting=0
  bNumEndpoints=2, bInterfaceClass=0x08 (Mass Storage)
  bInterfaceSubClass=0x06 (SCSI Transparent)
  bInterfaceProtocol=0x50 (Bulk-Only Transport)
  iInterface=4

端点 1 IN 描述符 (7B):
  bLength=7, bDescriptorType=5 (ENDPOINT)
  bEndpointAddress=0x81 (EP1, IN)
  bmAttributes=0x02 (Bulk)
  wMaxPacketSize=0x0040 (64B)
  bInterval=0

端点 2 OUT 描述符 (7B):
  bLength=7, bDescriptorType=5 (ENDPOINT)
  bEndpointAddress=0x02 (EP2, OUT)
  bmAttributes=0x02 (Bulk)
  wMaxPacketSize=0x0040 (64B)
  bInterval=0
```

#### 3.3.3 字符串描述符

| 索引 | 内容 | 用途 |
|---|---|---|
| 0 | 0x0409 (English US) | 语言 ID |
| 1 | "STMicroelectronics" | 厂商字符串 |
| 2 | "STM32 W25Q32 Flash Disk" | 产品字符串 |
| 3 | 12 位十六进制 (MCU UID) | 序列号 |
| 4 | "Mass Storage" | 接口字符串 |

### 3.4 设备属性管理 (`usb_prop`)

#### 3.4.1 核心数据结构

```c
/* 端点配置表 — USB 核心库通过 Device_Table 知道有多少端点和多少配置 */
DEVICE Device_Table = {
    EP_NUM,   // Total_Endpoint = 3 (EP0/EP1/EP2)
    1         // Total_Configuration = 1
};

/* 设备属性回调表 — USB 核心库在枚举各阶段调用这些函数指针 */
DEVICE_PROP Device_Property = {
    MASS_init,                    // ① Init — 上电时调用一次
    MASS_Reset,                   // ② Reset — 每次 USB RESET 时调用
    MASS_Status_In,               // ③ Process_Status_IN
    MASS_Status_Out,              // ④ Process_Status_OUT
    MASS_Data_Setup,              // ⑤ Class_Data_Setup — 类请求 (有数据阶段)
    MASS_NoData_Setup,            // ⑥ Class_NoData_Setup — 类请求 (无数据阶段)
    MASS_Get_Interface_Setting,   // ⑦ Class_Get_Interface_Setting
    MASS_GetDeviceDescriptor,     // ⑧ GetDeviceDescriptor — 返回 18B 描述符指针
    MASS_GetConfigDescriptor,     // ⑨ GetConfigDescriptor — 返回 32B 描述符指针
    MASS_GetStringDescriptor,     // ⑩ GetStringDescriptor — 返回字符串描述符指针
    NULL,                         // ⑪ RxEP_buffer — 使用默认缓冲区
    64                            // MaxPacketSize = 64
};

/* 标准请求回调表 — 标准设备请求的 handler */
USER_STANDARD_REQUESTS User_Standard_Requests = {
    MASS_GetConfiguration,        // GET_CONFIGURATION
    MASS_SetConfiguration,        // SET_CONFIGURATION
    MASS_GetInterface,            // GET_INTERFACE
    MASS_SetInterface,            // SET_INTERFACE
    MASS_GetStatus,               // GET_STATUS
    MASS_ClearFeature,            // CLEAR_FEATURE
    MASS_SetEndPointFeature,      // SET_FEATURE (Endpoint)
    MASS_SetDeviceFeature,        // SET_FEATURE (Device)
    MASS_SetDeviceAddress         // SET_ADDRESS
};
```

#### 3.4.2 关键回调实现要点

**`MASS_Reset()` — 最重要**：
```
MASS_Reset()
├── SetBTABLE(BTABLE_ADDRESS)                    // ① 设置缓冲区描述表基址
├── SetEPType(ENDP0, EP_CONTROL)                 // ② EP0 = 控制类型
├── SetEPTxStatus(ENDP0, EP_TX_NAK)              // ③ EP0 初始 NAK
├── SetEPRxAddr(ENDP0, ENDP0_RXADDR)            // ④ EP0 接收缓冲区地址
├── SetEPTxAddr(ENDP0, ENDP0_TXADDR)            // ⑤ EP0 发送缓冲区地址
├── Clear_Status_Out(ENDP0)                      // ⑥ 清除状态
├── SetEPRxValid(ENDP0)                          // ⑦ EP0 接收就绪
├── SetEPType(ENDP1, EP_BULK)                    // ⑧ EP1 = 批量类型
├── SetEPTxAddr(ENDP1, ENDP1_TXADDR)            // ⑨ EP1 发送地址
├── SetEPTxStatus(ENDP1, EP_TX_NAK)             // ⑩ EP1 初始 NAK
├── SetEPRxStatus(ENDP1, EP_RX_DIS)             // ⑪ EP1 禁收
├── SetEPType(ENDP2, EP_BULK)                    // ⑫ EP2 = 批量类型
├── SetEPRxAddr(ENDP2, ENDP2_RXADDR)            // ⑬ EP2 接收地址
├── SetEPRxStatus(ENDP2, EP_RX_VALID)           // ⑭ EP2 接收就绪
├── SetEPTxStatus(ENDP2, EP_TX_DIS)             // ⑮ EP2 禁发
├── SetBTABLE(BTABLE_ADDRESS)                    // ⑯ 再次确认
└── bDeviceState = ATTACHED                       // ⑰ 状态 → 已连接
```

> `MASS_Reset` 被 USB 核心库在检测到 USB RESET 信号时自动调用。USB RESET 是枚举流程的起点——主机在检测到 D+ 拉高后，会在 D+/D- 上保持 SE0 状态至少 10ms。

**`MASS_Data_Setup()` 和 `MASS_NoData_Setup()`**：
TR1 阶段 MSC 类请求（如 `Bulk-Only Mass Storage Reset` 0xFF, `Get Max LUN` 0xFE）可以返回 STALL。但标准请求（Get Descriptor 等）由 USB 核心库直接处理，不需要在此实现。

**描述符 getter**：三个描述符 getter 函数分别返回 `usb_desc.c` 中定义的描述符数组指针。

**`MASS_SetConfiguration()`**：
```
MASS_SetConfiguration()
├── if (配置值 != 0)
│       bDeviceState = CONFIGURED    // 枚举完成！
└── else
        bDeviceState = ADDRESSED
```

### 3.5 USB 中断服务 (`usb_istr`)

#### 3.5.1 中断链路

```
硬件 USB 中断 (USB_LP_CAN1_RX0_IRQn)
    │
    ▼
stm32f10x_it.c: USB_LP_CAN1_RX0_IRQHandler()
    │
    ▼
usb_core.c: USB_Istr()
    │  读取 ISTR 寄存器 → 解析事件源
    ├── RESET  → Device_Property.Reset()  → MASS_Reset()
    ├── CTR    → CTR_LP() / CTR_HP()      → 端点回调
    ├── SUSP   → Suspend()
    ├── WKUP   → Resume()
    ├── SOF    → SOF_Callback()
    └── ERR    → 错误处理
```

#### 3.5.2 数据结构

```c
/* usb_istr.c 中定义的全局变量 */
__IO uint16_t wIstr;                    // ISTR 寄存器缓存 (USB_Istr 写入)
__IO uint8_t  bIntPackSOF;              // SOF 计数器

/* 端点回调函数指针数组 — USB 核心库在 CTR 事件时按端点索引调用 */
void (*pEpInt_IN[7])(void) = {
    EP1_IN_Callback,                    // 端点 1 IN
    EP2_IN_Callback,                    // 端点 2 IN (NOP)
    EP3_IN_Callback,                    // (NOP)
    EP4_IN_Callback,                    // (NOP)
    EP5_IN_Callback,                    // (NOP)
    EP6_IN_Callback,                    // (NOP)
    EP7_IN_Callback,                    // (NOP)
};

void (*pEpInt_OUT[7])(void) = {
    EP1_OUT_Callback,                   // 端点 1 OUT (NOP)
    EP2_OUT_Callback,                   // 端点 2 OUT
    EP3_OUT_Callback,                   // (NOP)
    EP4_OUT_Callback,                   // (NOP)
    EP5_OUT_Callback,                   // (NOP)
    EP6_OUT_Callback,                   // (NOP)
    EP7_OUT_Callback,                   // (NOP)
};
```

> **TR1 注意**：`EP1_IN_Callback` 和 `EP2_OUT_Callback` 的实现可以是指向 `NOP_Process` 的空函数（通过 `usb_conf.h` 的宏绑定的就是 `NOP_Process`）。但回调数组必须使用与 `usb_conf.h` 中 `#define` 一致的名字，这样编译器才能正确解析。

### 3.6 电源管理 (`usb_pwr`)

#### 3.6.1 状态机

```
UNCONNECTED ──(PowerOn)──→ ATTACHED ──(RESET)──→ DEFAULT
    ↑                         ↑                      │
    │                         │              (SET_ADDRESS)
    │                         │                      ↓
    │                     (Cable_Config)         ADDRESSED
    │                         │                      │
    │                         │              (SET_CONFIGURATION)
    │                         │                      ↓
    └──(PowerOff)─────────────┴─────────────── CONFIGURED ←── 枚举完成!
```

#### 3.6.2 关键接口

```c
/* usb_pwr.c 中的全局状态 */
extern __IO uint32_t bDeviceState;    // USB 设备状态 (UNCONNECTED/ATTACHED/.../CONFIGURED)
extern __IO bool     fSuspendEnabled; // 是否允许挂起 (TR1 默认 FALSE, 避免 SUSP 中断进入 STOP)

/* 函数接口 */
void PowerOn(void);                   // 上电: 初始化 USB 核心, 连接 D+ 上拉
void PowerOff(void);                  // 断电: 断开 D+ 上拉, 禁用 USB 时钟
void Suspend(void);                   // 挂起处理
void Resume(RESUME_STATE eResumeSetVal); // 唤醒处理
```

**`PowerOn()` 核心流程**：
```
PowerOn()
├── USB_Cable_Config(ENABLE)          // ① PA12 切回 AF_PP → D+ 上拉生效
├── SetCNTR(CNTR_FRES)                // ② 强制复位 USB 外设
├── SetCNTR(0)                        // ③ 清除复位
├── SetISTR(0)                        // ④ 清除挂起的中断标志
├── wInterrupt_Mask = IMR_MSK         // ⑤ 设置中断掩码
├── SetCNTR(IMR_MSK)                  // ⑥ 使能中断 (CTR/RESET/SUSP/WKUP/...)
└── bDeviceState = ATTACHED           // ⑦ 状态 → 已连接
```

### 3.7 主程序入口 (`main`)

```c
#include "stm32f10x.h"
#include "hw_config.h"
#include "usb_lib.h"
#include "usb_pwr.h"

int main(void)
{
    Set_System();                   // ① 系统时钟 + GPIO + D+上拉引脚
    Set_USBClock();                 // ② USB 48MHz 专用时钟
    USB_Interrupts_Config();        // ③ NVIC 中断配置
    USB_Init();                     // ④ USB 核心库初始化 (内部调用 MASS_init → USB_SIL_Init)

    /* ── LED 初始化: PC13 推挽输出, 初始灭 (低电平点亮) ── */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    GPIO_SetBits(GPIOC, GPIO_Pin_13);  // LED 初始灭

    PowerOn();                      // ⑤ D+ 上拉使能 + USB 外设复位 + 中断使能

    /* ⑥ 等待 PC 完成枚举 */
    while (bDeviceState != CONFIGURED);

    GPIO_ResetBits(GPIOC, GPIO_Pin_13);  // LED 亮 — 枚举成功 (CONFIGURED)

    /* ⑦ 枚举完成 → 主循环 (中断驱动, 什么都不用做) */
    while (1)
    {
        /*
         * USB 核心库的所有后续处理都在 USB_Istr() 中完成,
         * USB_Istr() 由 USB_LP_CAN1_RX0_IRQHandler 调用
         */
    }
}
```

> **`PowerOn()` 由 main 显式调用**：`USB_Init()` → `MASS_init()` → `USB_SIL_Init()` 仅做设备表初始化和中断使能，不包含 D+ 上拉和 USB 外设 FRES 复位。`PowerOn()` 由 main 在 LED 初始化之后显式调用，确保 D+ 上拉前 GPIOC 已就绪。
>
> **`fSuspendEnabled = FALSE`**：TR1 阶段禁用挂起。主机在发 RESET 前的总线空闲期会触发 SUSP 中断，若 `fSuspendEnabled` 为 `TRUE` 则 MCU 进入 STOP 模式且无法唤醒（USBWakeUp 中断未配置），导致枚举彻底失败。

---

## 4. 数据流：从插入到检测

```
时间线          PC 侧                          STM32 侧
─────────────────────────────────────────────────────────────────
T0                                           MCU 上电
T1                                           main(): Set_System()
                                               → PA11/PA12 = AF_PP
T2                                           main(): Set_USBClock()
                                               → USB 48MHz 就绪
T3                                           main(): USB_Interrupts_Config()
                                               → NVIC 就绪
T4                                           main(): USB_Init()
                                               → PowerOn()
                                               → USB_Cable_Config(ENABLE)
                                               → PA12 恢复 AF_PP
                                               → D+ 被 R10 上拉到 3.3V
T5           集线器检测到 D+ 高 ────────────────── 硬件层面设备已"出现"
              ↓ 判定为全速设备 (12Mbps)
T6           主机发起 USB RESET
              (D+/D- 拉低 ≥10ms)  ──────────────→ USB 外设检测到 RESET
                                                   → ISTR RESET 中断
                                                   → USB_Istr() → Device_Property.Reset()
                                                   → MASS_Reset()
                                                   → 初始化 EP0/1/2
                                                   → bDeviceState = ATTACHED
T7           主机发送 SETUP 令牌                    ← EP0 就绪
              (GET_DESCRIPTOR Device, 8B)
T8                                               EP0 收到 SETUP 包
                                                   → USB_Istr() → CTR 事件
                                                   → USB 核心库解析为 GET_DESCRIPTOR
                                                   → MASS_GetDeviceDescriptor()
                                                   → 返回设备描述符前 8 字节
T9           主机收到 8 字节 ──────────────────→
              解析: USB 2.0, EP0=64B,
              VID=0x0483, PID=0x5720
T10          主机分配地址, 发送 SET_ADDRESS ────→ 设备记录地址
T11          主机再次 GET_DESCRIPTOR(Device,18B)→ 返回完整 18 字节
T12          主机 GET_DESCRIPTOR(Config, 9B)  ──→ 返回配置描述符前 9 字节
T13          主机 GET_DESCRIPTOR(Config, 32B) ──→ 返回完整 32 字节
T14          主机 GET_DESCRIPTOR(String...)   ──→ 返回字符串
T15          主机 SET_CONFIGURATION(1)        ──→ MASS_SetConfiguration()
                                                   → bDeviceState = CONFIGURED
T16          ✅ 设备管理器显示新设备              ✅ main() 退出等待循环
```

---

## 5. 文件清单：需新建 / 需修改

### 5.1 需新建的文件

| 文件 | 说明 |
|---|---|
| `inc/platform_config.h` | 平台配置宏 (USB_DISCONNECT 引脚、MCU 系列选择等) |
| `src/usb_desc.c` | USB 描述符数组 |
| `inc/usb_desc.h` | 描述符数组 extern 声明 |
| `src/usb_pwr.c` | 电源管理实现 |
| `inc/usb_pwr.h` | 电源管理头文件 |

### 5.2 需修改的文件

| 文件 | 修改内容 |
|---|---|
| `inc/hw_config.h` | 添加所有硬件初始化函数声明 + `BULK_MAX_PACKET_SIZE` 宏 |
| `inc/usb_conf.h` | 添加 `EP_NUM=3`、`BTABLE_ADDRESS`、端点缓冲区地址、端点回调宏 |
| `src/main.c` | 实现完整主流程 |
| `src/usb_prop.c` | 填入 `Device_Property` 和 `User_Standard_Requests` 的真实回调指针 + 回调实现 |
| `src/usb_istr.c` | 填入 `pEpInt_IN[7]` / `pEpInt_OUT[7]` 数组、添加 `bIntPackSOF` |
| `src/stm32f10x_it.c` | 添加 `USB_LP_CAN1_RX0_IRQHandler` → 调用 `USB_Istr()` |
| `project.uvprojx` | 添加新文件到工程编译列表 |

---

## 6. 验证方法

| 步骤 | 操作 | 预期结果 | 对应需求 |
|---|---|---|---|
| 1 | 编译烧录固件 | Keil 0 Error 0 Warning | — |
| 2 | USB 插入 PC USB 口 | Windows 发出设备插入提示音 | TR1-01 ✅ |
| 3 | 打开设备管理器 → 展开"通用串行总线控制器" | 出现 "STM32 W25Q32 Flash Disk" 或 "Unknown Device" (VID_0483&PID_5720) | TR1-01 ✅ TR1-02 |
| 4 | 打开 USBTreeView 查看设备节点 | 设备描述符字段与设计值一致 | TR1-02 ✅ |
| 5 | USBTreeView 查看配置描述符 | Mass Storage Class, Bulk-Only, 2 Endpoints | TR1-03 ✅ |
| 6 | USBTreeView 查看字符串描述符 | 厂商/产品/序列号正确 | TR1-04 ✅ |
| 7 | 拔掉 USB, 重新插入 | 设备重新出现 (软件重连) | TR1-05 ✅ |
| 8 | 观察枚举全程 | 无 STALL 错误, bDeviceState 到达 CONFIGURED | TR1-06 ✅ |

> **TR1-01 最小验收**：只要完成步骤 1~2，设备管理器出现新设备（即使是黄色感叹号），即满足 TR1-01。黄色感叹号是因为 TR1 没有实现 SCSI 命令处理（TR2 才做），属于正常现象。

---

## 7. 常见问题排查

| 现象 | 可能原因 | 排查方向 |
|---|---|---|
| PC 完全无反应 | D+ 上拉被 PA12 拉低 | ① 测量 D+ 对地电压，应为 ~3.3V（若为 0V，说明 PA12 仍为 GPIO 输出低） ② 确认 USB_Cable_Config(ENABLE) 已被调用 |
| | USB 48MHz 时钟错误 | 用示波器测 D+/D- 有无 USB 信号 |
| | GPIO 模式错误 | PA12 必须已切回 AF_PP；PA11 也必须是 AF_PP |
| 设备管理器出现 "Unknown Device" | 设备描述符返回失败 | ① 检查 MASS_Reset 是否正确初始化 EP0 ② EP0 缓冲区地址是否与 PMA 布局匹配 |
| | 描述符内容非法 | ② 用 USB 分析仪抓包检查返回数据 |
| 枚举到一半 STALL | 配置描述符 wTotalLength 与实际不一致 | 确保 MASS_SIZ_CONFIG_DESC == 32 |
| | 字符串描述符索引越界 | 设备描述符中 iManufacturer/iProduct/iSerial 不能超过实际字符串数量 |
| 代码卡死 | USB 中断未响应 | 检查 usb_istr.c 中是否有 USB_LP_CAN1_RX0_IRQHandler |
| | NVIC 未使能 | 检查 USB_Interrupts_Config 的 NVIC_IRQChannel |

---

*文档结束。*
