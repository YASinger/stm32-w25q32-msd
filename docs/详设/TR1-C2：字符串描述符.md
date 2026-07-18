# TR1-C2 详细设计：字符串描述符

## 修改记录

| 版本 | 日期 | 修改内容 | 修改人 |
|---|---|---|---|
| V1.0 | 2026-07-19 | 初始版本 | Copilot |

---

## 文档信息

| 项目 | 内容 |
|---|---|
| 需求编号 | TR1-C2 |
| 需求描述 | 字符串描述符（厂商/产品/序列号，序列号读 MCU UID 生成 12 位十六进制） |
| 验收标准 | USBTreeView 显示 "STMicroelectronics" / "STM32 W25Q32 Flash Disk" / 唯一 SN |
| 所属阶段 | TR1-C — 枚举完整化（横向补全） |
| 前置文档 | `项目框架设计.md`、`TR1框架设计.md` |
| 前置代码 | TR1-C1 配置描述符已完成，设备被识别为 USB 大容量存储设备 |
| 完成态参照 | `tmp\last_project\src\usb_desc.c`、`tmp\last_project\src\hw_config.c`、`tmp\last_project\inc\hw_config.h` |

---

## 1. 需求分解

TR1-C2 的目标是补入 4 组字符串描述符（语言 ID/厂商/产品/序列号），让 USBTreeView 能显示设备的厂商、产品名和唯一序列号。同时实现 `Get_SerialNum()` 从 MCU 96-bit UID 生成序列号。

**C2 的可观测变化**：C1 完成后 USBTreeView 的 String Descriptors 部分显示"not available"。C2 完成后显示 5 组字符串：
- String 0：语言 ID（0x0409 English US）
- String 1："STMicroelectronics"（厂商）
- String 2："STM32 W25Q32 Flash Disk"（产品）
- String 3：12 位十六进制序列号（读 MCU UID 生成，每块板子不同）
- String 4："ST Mass"（接口字符串）

本任务需要修改 4 个文件：

| 操作 | 文件 | 内容 |
|---|---|---|
| 修改 | `inc/usb_desc.h` | 补 5 个字符串大小宏 + 5 个数组声明 |
| 修改 | `src/usb_desc.c` | 补 5 个字符串描述符数组 |
| 修改 | `inc/hw_config.h` | 补 `Get_SerialNum()` 声明 |
| 修改 | `src/hw_config.c` | 补 `Get_SerialNum()` + `IntToUnicode()` 实现 |
| 修改 | `src/usb_prop.c` | 补 `String_Descriptor[5]` 包装 + `MASS_GetStringDescriptor` 回调 + `Device_Property.GetStringDescriptor` 恢复 |
| 修改 | `src/main.c` | 加 `Get_SerialNum()` 调用 |

---

## 2. 关键技术决策

### 2.1 字符串描述符的 Unicode 编码

USB 字符串描述符使用 **UTF-16LE**（小端序 Unicode）编码，每个字符占 2 字节。例如 'S' 的 ASCII 是 0x53，Unicode 是 0x0053，存储为 `{0x53, 0x00}`。

数组结构：
```
[bLength] [0x03] [字符1低] [字符1高] [字符2低] [字符2高] ...
```

- `bLength` = 2 + 字符数 × 2（描述符头 2 字节 + 每字符 2 字节）
- `0x03` = STRING 描述符类型

### 2.2 5 组字符串的内容与大小

| 索引 | 内容 | 字符数 | 字节数 | 宏 |
|---|---|---|---|---|
| 0 | 语言 ID | — | 4 | `MASS_SIZ_STRING_LANGID` |
| 1 | "STMicroelectronics" | 18 | 38 | `MASS_SIZ_STRING_VENDOR` |
| 2 | "STM32 W25Q32 Flash Disk" | 23 | 48 | `MASS_SIZ_STRING_PRODUCT` |
| 3 | 序列号（运行时填充） | 12 | 26 | `MASS_SIZ_STRING_SERIAL` |
| 4 | "ST Mass" | 7 | 16 | `MASS_SIZ_STRING_INTERFACE` |

**大小计算**：`bLength = 2 + 字符数 × 2`。例如 "STMicroelectronics" 18 字符 → 2 + 36 = 38 字节。

### 2.3 序列号的 MCU UID 生成（§6.7 的唯一 SN 要求）

STM32F10x 有 96-bit 唯一 ID，存储在 3 个 32 位寄存器中：

| 宏 | 地址 | 内容 |
|---|---|---|
| `ID1` | 0x1FFFF7E8 | UID[31:0] |
| `ID2` | 0x1FFFF7EC | UID[63:32] |
| `ID3` | 0x1FFFF7F0 | UID[95:64] |

`Get_SerialNum()` 的算法（来自完成态）：
1. 读 3 个 UID 寄存器
2. `Device_Serial0 += Device_Serial2`（混合 UID 高低位，增加离散性）
3. 用 `IntToUnicode()` 把 `Device_Serial0` 转 8 位十六进制（16 字符），`Device_Serial1` 转 4 位十六进制（8 字符）
4. 共 12 位十六进制字符，填入 `MASS_StringSerial[2]` 和 `MASS_StringSerial[18]`

**`IntToUnicode()` 的逻辑**：每次取 32 位值的最高 4 位（1 个十六进制位），转成 '0'~'9' 或 'A'~'F'，写入 2 字节（字符 + 0x00）。循环 `len` 次，每次左移 4 位。

### 2.4 `MASS_StringSerial` 的非 const 属性

其他 4 组字符串是 `const`（运行时不变，存储在 Flash）。但 `MASS_StringSerial` 是 `uint8_t`（非 const），因为 `Get_SerialNum()` 在运行时修改它（读 UID 填充）。这意味着 `MASS_StringSerial` 存储在 RAM 而非 Flash。

**`Get_SerialNum()` 的调用时机**：必须在 `USB_Init()` 之前调用——因为 `USB_Init()` → `MASS_init()` 可能会触发枚举，主机请求字符串描述符时 `MASS_StringSerial` 必须已填充。

### 2.5 `MASS_GetStringDescriptor` 的索引分发

主机请求字符串描述符时，`wValue` 低字节是字符串索引（0~4）。`MASS_GetStringDescriptor()` 根据 `pInformation->USBwValue0` 从 `String_Descriptor[5]` 数组中选对应项：

```c
uint8_t index = pInformation->USBwValue0;
if (index < 5) {
    return Standard_GetDescriptorData(Length, &String_Descriptor[index]);
}
return NULL;
```

---

## 3. 涉及组件

```
┌─────────────────────────────────────────────────┐
│  主机 GET_DESCRIPTOR(String) 请求                │
└────────────────────┬────────────────────────────┘
                     │ EP0 控制传输
╔════════════════════╧═════════════════════════════╗
║  desc (USB 设备层)                                ║
║  ┌───────────────────────────────────────────┐  ║
║  │ usb_desc.c                                │  ║
║  │  ├─ MASS_StringLangID[4]      语言 ID    │  ║
║  │  ├─ MASS_StringVendor[38]     厂商       │  ║
║  │  ├─ MASS_StringProduct[48]    产品       │  ║
║  │  ├─ MASS_StringSerial[26]     序列号(RAM)│  ║
║  │  └─ MASS_StringInterface[16]  接口       │  ║
║  └───────────────────────────────────────────┘  ║
╠══════════════════════════════════════════════════╣
║  hwinit (硬件驱动层)                              ║
║  ┌───────────────────────────────────────────┐  ║
║  │ hw_config.c                               │  ║
║  │  └─ Get_SerialNum()  读 MCU UID 填序列号  │  ║
║  └───────────────────────────────────────────┘  ║
╠══════════════════════════════════════════════════╣
║  device (USB 设备层)                              ║
║  ┌───────────────────────────────────────────┐  ║
║  │ usb_prop.c                                │  ║
║  │  ├─ String_Descriptor[5]      描述符包装  │  ║
║  │  ├─ MASS_GetStringDescriptor() 回调       │  ║
║  │  └─ Device_Property.GetStringDescriptor   │  ║
║  │     从 0 → MASS_GetStringDescriptor       │  ║
║  └───────────────────────────────────────────┘  ║
╚══════════════════════════════════════════════════╝
```

---

## 4. 文件清单

### 4.1 修改文件

| 文件 | 修改内容 |
|---|---|
| `inc/usb_desc.h` | 补 5 个字符串大小宏 + 5 个数组声明 |
| `src/usb_desc.c` | 补 5 个字符串描述符数组 |
| `inc/hw_config.h` | 补 `Get_SerialNum()` 声明 |
| `src/hw_config.c` | 补 `Get_SerialNum()` + `IntToUnicode()` 实现 + UID 地址宏 |
| `src/usb_prop.c` | 补 `String_Descriptor[5]` 包装 + 前向声明 + `MASS_GetStringDescriptor` 实现 + `Device_Property.GetStringDescriptor` 恢复 |
| `src/main.c` | `USB_Init()` 前加 `Get_SerialNum()` 调用 |

---

## 5. 接口设计

### 5.1 `inc/usb_desc.h` 修改

在现有 `MASS_ConfigDescriptor` 声明后，补入：

```c
#define MASS_SIZ_STRING_LANGID            4
#define MASS_SIZ_STRING_VENDOR            38
#define MASS_SIZ_STRING_PRODUCT           48
#define MASS_SIZ_STRING_SERIAL            26
#define MASS_SIZ_STRING_INTERFACE         16

extern const uint8_t MASS_StringLangID[MASS_SIZ_STRING_LANGID];
extern const uint8_t MASS_StringVendor[MASS_SIZ_STRING_VENDOR];
extern const uint8_t MASS_StringProduct[MASS_SIZ_STRING_PRODUCT];
extern uint8_t MASS_StringSerial[MASS_SIZ_STRING_SERIAL];    /* 非 const: 运行时填 UID */
extern const uint8_t MASS_StringInterface[MASS_SIZ_STRING_INTERFACE];
```

### 5.2 `src/usb_desc.c` 修改

在现有 `MASS_ConfigDescriptor` 数组后，补入 5 个字符串数组：

```c
/* ── 字符串描述符 ─────────────────────────────────────────────────────────── */

/* 语言 ID */
const uint8_t MASS_StringLangID[MASS_SIZ_STRING_LANGID] = {
    MASS_SIZ_STRING_LANGID,
    0x03,                     /* bDescriptorType = STRING */
    0x09, 0x04                /* LangID = 0x0409 (English US) */
};

/* 厂商字符串: "STMicroelectronics" */
const uint8_t MASS_StringVendor[MASS_SIZ_STRING_VENDOR] = {
    MASS_SIZ_STRING_VENDOR,
    0x03,
    'S', 0, 'T', 0, 'M', 0, 'i', 0, 'c', 0, 'r', 0, 'o', 0, 'e', 0,
    'l', 0, 'e', 0, 'c', 0, 't', 0, 'r', 0, 'o', 0, 'n', 0, 'i', 0,
    'c', 0, 's', 0
};

/* 产品字符串: "STM32 W25Q32 Flash Disk" */
const uint8_t MASS_StringProduct[MASS_SIZ_STRING_PRODUCT] = {
    MASS_SIZ_STRING_PRODUCT,
    0x03,
    'S', 0, 'T', 0, 'M', 0, '3', 0, '2', 0, ' ', 0,
    'W', 0, '2', 0, '5', 0, 'Q', 0, '3', 0, '2', 0, ' ', 0,
    'F', 0, 'l', 0, 'a', 0, 's', 0, 'h', 0, ' ', 0,
    'D', 0, 'i', 0, 's', 0, 'k', 0
};

/* 序列号 (运行时由 Get_SerialNum 填充) */
uint8_t MASS_StringSerial[MASS_SIZ_STRING_SERIAL] = {
    MASS_SIZ_STRING_SERIAL,
    0x03,
    'S', 0, 'T', 0, 'M', 0, '3', 0, '2', 0, ' ', 0,
    '0', 0, '0', 0, '0', 0, '0', 0, '0', 0, '0', 0
};

/* 接口字符串: "ST Mass" */
const uint8_t MASS_StringInterface[MASS_SIZ_STRING_INTERFACE] = {
    MASS_SIZ_STRING_INTERFACE,
    0x03,
    'S', 0, 'T', 0, ' ', 0, 'M', 0, 'a', 0, 's', 0, 's', 0
};
```

### 5.3 `inc/hw_config.h` 修改

补入 `Get_SerialNum()` 声明：

```c
void Get_SerialNum(void);
```

### 5.4 `src/hw_config.c` 修改

在文件顶部补 UID 地址宏和 `IntToUnicode` 前向声明，在文件末尾补两个函数实现：

```c
/* 文件顶部 (include 后) */
#include "usb_desc.h"

#define ID1     (0x1FFFF7E8)
#define ID2     (0x1FFFF7EC)
#define ID3     (0x1FFFF7F0)

static void IntToUnicode(uint32_t value, uint8_t *pbuf, uint8_t len);

/* 文件末尾 */
static void IntToUnicode(uint32_t value, uint8_t *pbuf, uint8_t len)
{
    uint8_t idx;
    for (idx = 0; idx < len; idx++) {
        if ((value >> 28) < 0xA) {
            pbuf[2 * idx] = (uint8_t)((value >> 28) + '0');
        } else {
            pbuf[2 * idx] = (uint8_t)((value >> 28) + 'A' - 10);
        }
        value <<= 4;
        pbuf[2 * idx + 1] = 0;
    }
}

void Get_SerialNum(void)
{
    uint32_t Device_Serial0, Device_Serial1, Device_Serial2;

    Device_Serial0 = *(uint32_t *)ID1;
    Device_Serial1 = *(uint32_t *)ID2;
    Device_Serial2 = *(uint32_t *)ID3;

    Device_Serial0 += Device_Serial2;

    if (Device_Serial0 != 0) {
        IntToUnicode(Device_Serial0, &MASS_StringSerial[2], 8);
        IntToUnicode(Device_Serial1, &MASS_StringSerial[18], 4);
    }
}
```

**注意**：`hw_config.c` 需要新加 `#include "usb_desc.h"`（访问 `MASS_StringSerial`）。

### 5.5 `src/usb_prop.c` 修改

**改动 1**：补 `String_Descriptor[5]` 包装（在 `Config_Descriptor` 后）：

```c
static ONE_DESCRIPTOR String_Descriptor[5] = {
    {(uint8_t *)MASS_StringLangID,    MASS_SIZ_STRING_LANGID},
    {(uint8_t *)MASS_StringVendor,    MASS_SIZ_STRING_VENDOR},
    {(uint8_t *)MASS_StringProduct,   MASS_SIZ_STRING_PRODUCT},
    {(uint8_t *)MASS_StringSerial,    MASS_SIZ_STRING_SERIAL},
    {(uint8_t *)MASS_StringInterface, MASS_SIZ_STRING_INTERFACE},
};
```

**改动 2**：补前向声明：

```c
static uint8_t *MASS_GetStringDescriptor(uint16_t Length);
```

**改动 3**：`Device_Property.GetStringDescriptor` 从 0 改为 `MASS_GetStringDescriptor`：

```c
/* 改前 */
0,                            /* GetStringDescriptor — C2 实现 */
/* 改后 */
MASS_GetStringDescriptor,    /* GetStringDescriptor */
```

**改动 4**：补 `MASS_GetStringDescriptor` 实现（在 `MASS_GetConfigDescriptor` 后）：

```c
static uint8_t *MASS_GetStringDescriptor(uint16_t Length)
{
    uint8_t index = pInformation->USBwValue0;
    uint8_t *pBuf = NULL;

    if (index < 5) {
        pBuf = Standard_GetDescriptorData(Length, &String_Descriptor[index]);
    }

    return pBuf;
}
```

### 5.6 `src/main.c` 修改

在 `USB_Init()` 前加 `Get_SerialNum()`：

```c
Set_System();
Set_USBClock();
USB_Interrupts_Config();
Get_SerialNum();              /* C2: 用 MCU UID 填充序列号字符串 */
USB_Init();
PowerOn();
```

**调用顺序**：`Get_SerialNum()` 必须在 `USB_Init()` 之前——`USB_Init()` 触发枚举后主机可能立即请求字符串描述符，`MASS_StringSerial` 必须已填充。

---

## 6. 实现要点与风险

### 6.1 C2 的可观测变化

**C1 → C2 的 USBTreeView 变化**：

| 项 | C1 | C2 | 变化 |
|---|---|---|---|
| String Descriptors | not available | **5 组完整** | ✅ |
| Manufacturer | 无 | **"STMicroelectronics"** | ✅ |
| Product | 无 | **"STM32 W25Q32 Flash Disk"** | ✅ |
| Serial | 无 | **12 位十六进制（每板不同）** | ✅ |
| Interface | 无 | **"ST Mass"** | ✅ |
| Problem Code | 10 | 10（不变） | C4 才解决 |

### 6.2 序列号的唯一性验证

每块 STM32 的 96-bit UID 不同，`Get_SerialNum()` 生成的 12 位十六进制序列号也不同。验证方法：
- 烧录到不同板子，USBTreeView 应显示不同序列号
- 序列号格式：`XXXXXXXXXXXXXXXXXXXXXXXX`（24 个十六进制字符，12 字节 × 2）

**注意**：`IntToUnicode(Device_Serial0, ..., 8)` 生成 8 个十六进制位（16 字符），`IntToUnicode(Device_Serial1, ..., 4)` 生成 4 个十六进制位（8 字符），共 12 位十六进制（24 字符）。

### 6.3 `MASS_StringSerial` 的 RAM 开销

`MASS_StringSerial[26]` 存储在 RAM（非 const），占用 26 字节。STM32F103C8 有 20KB SRAM，这点开销可忽略。

### 6.4 `Get_SerialNum()` 的调用时机

必须在 `USB_Init()` 之前。若在之后调用，主机可能在 `Get_SerialNum()` 执行前请求字符串描述符，拿到的是初始占位值 "STM32 000000"。

---

## 7. 验证流程

### 7.1 编译验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | Keil Build (F7) | 0 Error(s), 0 Warning(s) | 待验证 |

### 7.2 USBTreeView 验证（核心验收）

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 烧录，插 USB | 设备管理器仍显示"USB 大容量存储设备" | 待验证 |
| 2 | USBTreeView String Descriptors | 5 组完整可读 | **C2 核心验收** |
| 3 | Manufacturer String | "STMicroelectronics" | **C2 核心验收** |
| 4 | Product String | "STM32 W25Q32 Flash Disk" | **C2 核心验收** |
| 5 | Serial | 12 位十六进制（非 "STM32 000000"） | **C2 核心验收** |
| 6 | Interface String | "ST Mass" | 待验证 |

### 7.3 序列号唯一性验证（可选）

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 板 A 烧录，记录序列号 | 12 位十六进制 | — |
| 2 | 板 B 烧录，记录序列号 | 与板 A 不同 | 待验证 |

---

## 8. 常见问题排查

### 8.1 编译报错：MASS_StringSerial 未定义

**原因**：`usb_desc.c` 未补 `MASS_StringSerial` 数组，或 `hw_config.c` 未 `#include "usb_desc.h"`。

**修复**：确认 `usb_desc.c` 已补 5 个字符串数组，`hw_config.c` 顶部有 `#include "usb_desc.h"`。

### 8.2 USBTreeView 序列号显示 "STM32 000000"

**原因**：`Get_SerialNum()` 未被调用，或调用在 `USB_Init()` 之后。

**修复**：确认 `main.c` 在 `USB_Init()` 前调用 `Get_SerialNum()`（§5.6）。

### 8.3 USBTreeView 字符串乱码

**原因**：字符串数组的 Unicode 编码错误，每个字符应为 `{ASCII, 0x00}` 两字节。

**修复**：检查数组内容，每个字符后跟 `0`（如 `'S', 0, 'T', 0`）。

### 8.4 序列号全 0

**原因**：UID 寄存器地址错误，或 `Device_Serial0 += Device_Serial2` 后恰好为 0（概率极低）。

**修复**：确认 `ID1=0x1FFFF7E8`/`ID2=0x1FFFF7EC`/`ID3=0x1FFFF7F0`。若 `Device_Serial0 == 0`，`Get_SerialNum()` 不填充，序列号保持初始值。

---

## 9. 与下一步的衔接

TR1-C2 完成后，USBTreeView 显示完整字符串描述符。下一步：

- **TR1-C3**：`usb_prop.c` 实现 `Mass_Storage_SetConfiguration()`（`bDeviceState = CONFIGURED` + 清 EP1/EP2 DTOG）+ `MASS_Get_Interface_Setting()`。B4 的 `while (bDeviceState != CONFIGURED)` 退出。
- **TR1-C4**：`usb_prop.c` 补 `MASS_Data_Setup`/`MASS_NoData_Setup`（MSC 类请求 GET_MAX_LUN / Bulk-Only Reset）。Problem Code 10 消失，USBSTOR 驱动完全加载。

> **C2 是"装修"阶段的第一步**：C1 让设备"立起来"（从未知变成大容量存储设备），C2 让设备"有名字"（厂商/产品/序列号）。C3/C4 解决功能层问题（状态机/类请求），C5 解决软件重连。

---

*文档结束。*
