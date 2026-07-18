# TR1-C1 详细设计：配置描述符

## 修改记录

| 版本 | 日期 | 修改内容 | 修改人 |
|---|---|---|---|
| V1.0 | 2026-07-19 | 初始版本 | Copilot |

---

## 文档信息

| 项目 | 内容 |
|---|---|
| 需求编号 | TR1-C1 |
| 需求描述 | 配置描述符（9B 配置 + 9B 接口 + 7B EP1 + 7B EP2，MSC/Bulk-Only/自供电） |
| 验收标准 | USBTreeView 显示 Mass Storage Class, 2 Bulk EP；黄色感叹号消失（USBSTOR 驱动可加载） |
| 所属阶段 | TR1-C — 枚举完整化（横向补全） |
| 前置文档 | `项目框架设计.md`、`TR1框架设计.md` |
| 前置代码 | TR1-A 骨架全部完成；TR1-B 最小枚举通过（B1~B4） |
| 完成态参照 | `tmp\last_project\src\usb_desc.c`、`tmp\last_project\src\usb_prop.c` |

---

## 1. 需求分解

TR1-C1 是 TR1-C 逐层补全的第一步，目标是补入 32 字节配置描述符，让主机的 `GET_DESCRIPTOR(Config)` 能拿到完整的配置/接口/端点描述符。

**C1 是 TR1 阶段可观测变化最显著的任务之一**：B2~B4 阶段 Problem Code 43（设备启动失败）的根因就是 `GetConfigDescriptor = NULL`。C1 补入配置描述符后，主机 GET_DESCRIPTOR(Config) 成功，黄色感叹号消失，USBSTOR 驱动可加载——设备从"未知 USB 设备"变成"USB 大容量存储设备"。

本任务需要修改两个现有文件：

| 操作 | 文件 | 内容 |
|---|---|---|
| 修改 | `src/usb_desc.c` | 补 `MASS_ConfigDescriptor` 数组（32B） |
| 修改 | `inc/usb_desc.h` | 补 `MASS_SIZ_CONFIG_DESC` 宏 + `MASS_ConfigDescriptor` 声明 |
| 修改 | `src/usb_prop.c` | 补 `Config_Descriptor` 包装 + `MASS_GetConfigDescriptor` 回调，`Device_Property.GetConfigDescriptor` 从 0 改为 `MASS_GetConfigDescriptor` |

**与完成态的关系**：完成态的 `usb_desc.c` 和 `usb_prop.c` 已包含配置描述符及其回调，C1 直接恢复这些内容，无需裁剪。

---

## 2. 关键技术决策

### 2.1 配置描述符结构（32 字节）

配置描述符是一个复合结构，包含 4 个子描述符：

```
配置描述符 (9B)  → 接口描述符 (9B) → 端点1 IN 描述符 (7B) → 端点2 OUT 描述符 (7B)
```

**各字段与文档 §6.7 的对照**：

| 子描述符 | 字段 | 值 | 说明 |
|---|---|---|---|
| 配置 | bLength | 0x09 | 9 字节 |
| | bDescriptorType | 0x02 | CONFIGURATION |
| | wTotalLength | 0x0020 (32) | 总长度 32 字节 |
| | bNumInterfaces | 0x01 | 1 个接口 |
| | bConfigurationValue | 0x01 | 配置 1 |
| | iConfiguration | 0x00 | 无字符串 |
| | bmAttributes | 0xC0 | 自供电（D6=1） |
| | MaxPower | 0x32 (100mA) | 最大功耗 |
| 接口 | bLength | 0x09 | 9 字节 |
| | bDescriptorType | 0x04 | INTERFACE |
| | bInterfaceNumber | 0x00 | 接口 0 |
| | bAlternateSetting | 0x00 | 无备选设置 |
| | bNumEndpoints | 0x02 | 2 个端点 |
| | bInterfaceClass | 0x08 | Mass Storage |
| | bInterfaceSubClass | 0x06 | SCSI transparent |
| | bInterfaceProtocol | 0x50 | Bulk-Only Transport |
| | iInterface | 0x04 | 字符串索引 4 |
| 端点1 IN | bLength | 0x07 | 7 字节 |
| | bDescriptorType | 0x05 | ENDPOINT |
| | bEndpointAddress | 0x81 | EP1, IN |
| | bmAttributes | 0x02 | Bulk |
| | wMaxPacketSize | 0x0040 (64) | 64 字节 |
| | bInterval | 0x00 | 忽略 |
| 端点2 OUT | bLength | 0x07 | 7 字节 |
| | bDescriptorType | 0x05 | ENDPOINT |
| | bEndpointAddress | 0x02 | EP2, OUT |
| | bmAttributes | 0x02 | Bulk |
| | wMaxPacketSize | 0x0040 (64) | 64 字节 |
| | bInterval | 0x00 | 忽略 |

### 2.2 `iInterface = 4` 的依赖

接口描述符中 `iInterface = 4` 指向字符串描述符索引 4（接口字符串 "ST Mass"）。C2 才补字符串描述符，C1 阶段索引 4 不存在。

**处理**：C1 保留 `iInterface = 4`（与完成态一致）。主机读配置描述符时拿到索引 4，尝试 GET_DESCRIPTOR(String 4)，若 C2 未完成会失败——但**这不会导致黄色感叹号**。主机对字符串描述符的容错较高，拿不到接口字符串只会显示空字符串，不影响设备功能。黄色感叹号的根因是配置描述符缺失，不是字符串缺失。

### 2.3 `MASS_Reset()` 中 `Current_Feature` 的恢复

B2 阶段 `MASS_Reset()` 中 `pInformation->Current_Feature` 硬编码为 `0xC0`（因为 `MASS_ConfigDescriptor` 不存在）。C1 补入配置描述符后，改回 `MASS_ConfigDescriptor[7]`（配置描述符的第 7 字节，即 `bmAttributes` 字段）。

### 2.4 `Device_Property.GetConfigDescriptor` 的恢复

B2 阶段 `Device_Property` 的 `GetConfigDescriptor` 字段为 0（NULL）。C1 改为 `MASS_GetConfigDescriptor`，主机 GET_DESCRIPTOR(Config) 时 USB 库调用该回调返回配置描述符。

---

## 3. 涉及组件

```
┌─────────────────────────────────────────────────┐
│  主机 GET_DESCRIPTOR(Config) 请求                │
└────────────────────┬────────────────────────────┘
                     │ EP0 控制传输
╔════════════════════╧═════════════════════════════╗
║  desc (USB 设备层)                                ║
║  ┌───────────────────────────────────────────┐  ║
║  │ usb_desc.c                                │  ║
║  │  └─ MASS_ConfigDescriptor[32]  配置描述符 │  ║
║  └───────────────────────────────────────────┘  ║
╠══════════════════════════════════════════════════╣
║  device (USB 设备层)                              ║
║  ┌───────────────────────────────────────────┐  ║
║  │ usb_prop.c                                │  ║
║  │  ├─ Config_Descriptor        描述符包装   │  ║
║  │  ├─ MASS_GetConfigDescriptor() 回调       │  ║
║  │  └─ Device_Property.GetConfigDescriptor   │  ║
║  │     从 0 → MASS_GetConfigDescriptor       │  ║
║  └───────────────────────────────────────────┘  ║
╚══════════════════════════════════════════════════╝
```

---

## 4. 文件清单

### 4.1 修改文件

| 文件 | 修改内容 |
|---|---|
| `src/usb_desc.c` | 补 `MASS_ConfigDescriptor` 数组（32B） |
| `inc/usb_desc.h` | 补 `MASS_SIZ_CONFIG_DESC` 宏 + `MASS_ConfigDescriptor` 声明 |
| `src/usb_prop.c` | 补 `Config_Descriptor` 包装 + `MASS_GetConfigDescriptor` 回调 + `Device_Property.GetConfigDescriptor` 恢复 + `MASS_Reset()` 中 `Current_Feature` 改回 `MASS_ConfigDescriptor[7]` |

### 4.2 新建文件

无。

### 4.3 工程分组

无变化。

---

## 5. 接口设计

### 5.1 `inc/usb_desc.h` 修改

在现有 `MASS_SIZ_DEVICE_DESC` 和 `MASS_DeviceDescriptor` 声明后，补入：

```c
#define MASS_SIZ_CONFIG_DESC              32

extern const uint8_t MASS_ConfigDescriptor[MASS_SIZ_CONFIG_DESC];
```

### 5.2 `src/usb_desc.c` 修改

在现有 `MASS_DeviceDescriptor` 数组后，补入：

```c
/* ── 配置描述符 (32 字节) ─────────────────────────────────────────────────── */
const uint8_t MASS_ConfigDescriptor[MASS_SIZ_CONFIG_DESC] = {
    /******** 配置描述符 (9B) ********/
    0x09,   /* bLength                                          */
    0x02,   /* bDescriptorType = CONFIGURATION                  */
    MASS_SIZ_CONFIG_DESC,                                       /* wTotalLength    */
    0x00,
    0x01,   /* bNumInterfaces = 1                               */
    0x01,   /* bConfigurationValue = 1                          */
    0x00,   /* iConfiguration                                   */
    0xC0,   /* bmAttributes  = 自供电                           */
    0x32,   /* bMaxPower     = 100 mA                           */

    /******** 接口描述符 (9B) ********/
    0x09,   /* bLength                                          */
    0x04,   /* bDescriptorType = INTERFACE                      */
    0x00,   /* bInterfaceNumber                                 */
    0x00,   /* bAlternateSetting                                */
    0x02,   /* bNumEndpoints                                    */
    0x08,   /* bInterfaceClass    = Mass Storage                */
    0x06,   /* bInterfaceSubClass = SCSI transparent            */
    0x50,   /* bInterfaceProtocol = Bulk-Only Transport         */
    4,      /* iInterface                                       */

    /******** 端点 1 IN 描述符 (7B) ********/
    0x07,   /* bLength                                          */
    0x05,   /* bDescriptorType = ENDPOINT                       */
    0x81,   /* bEndpointAddress = EP1, IN                       */
    0x02,   /* bmAttributes      = Bulk                         */
    0x40,   /* wMaxPacketSize    = 64                           */
    0x00,
    0x00,   /* bInterval                                       */

    /******** 端点 2 OUT 描述符 (7B) ********/
    0x07,   /* bLength                                          */
    0x05,   /* bDescriptorType = ENDPOINT                       */
    0x02,   /* bEndpointAddress = EP2, OUT                      */
    0x02,   /* bmAttributes      = Bulk                         */
    0x40,   /* wMaxPacketSize    = 64                           */
    0x00,
    0x00    /* bInterval                                       */
};
```

### 5.3 `src/usb_prop.c` 修改

**改动 1**：补 `Config_Descriptor` 包装（在 `Device_Descriptor` 后）：

```c
static ONE_DESCRIPTOR Config_Descriptor = {
    (uint8_t *)MASS_ConfigDescriptor,
    MASS_SIZ_CONFIG_DESC
};
```

**改动 2**：补前向声明（在 `MASS_GetDeviceDescriptor` 声明后）：

```c
static uint8_t *MASS_GetConfigDescriptor(uint16_t Length);
```

**改动 3**：`Device_Property` 的 `GetConfigDescriptor` 字段从 0 改为 `MASS_GetConfigDescriptor`：

```c
/* 改前 */
0,                            /* GetConfigDescriptor — C1 实现 */
/* 改后 */
MASS_GetConfigDescriptor,    /* GetConfigDescriptor */
```

**改动 4**：`MASS_Reset()` 中 `Current_Feature` 改回 `MASS_ConfigDescriptor[7]`：

```c
/* 改前 */
/* pInformation->Current_Feature = MASS_ConfigDescriptor[7]; */  /* C1 实现 */
pInformation->Current_Feature = 0xC0;  /* B2 硬编码: 自供电 (§6.7 bmAttributes) */
/* 改后 */
pInformation->Current_Feature = MASS_ConfigDescriptor[7];
```

**改动 5**：补 `MASS_GetConfigDescriptor` 实现（在 `MASS_GetDeviceDescriptor` 后）：

```c
static uint8_t *MASS_GetConfigDescriptor(uint16_t Length)
{
    return Standard_GetDescriptorData(Length, &Config_Descriptor);
}
```

---

## 6. 实现要点与风险

### 6.1 C1 的可观测变化（最显著）

**B4 → C1 的 USBTreeView 变化**：

| 项 | B4 | C1 | 变化 |
|---|---|---|---|
| Problem Code | 43 | **消失** | ✅ 黄色感叹号消失 |
| Device Description | 未知 USB 设备(设备描述符请求失败) | **USB 大容量存储设备** | ✅ 设备类型识别 |
| Driver | 无 | **USBSTOR.SYS** | ✅ 存储驱动加载 |
| Used Endpoints | 1 | **3** | ✅ EP0+EP1+EP2 全部激活 |
| Configuration Descriptor | 无 | **完整 32B** | ✅ 配置描述符可读 |
| Interface Class | 无 | **Mass Storage** | ✅ MSC 识别 |

**C1 是 TR1 阶段可观测变化最显著的任务**——从"未知 USB 设备（带黄色感叹号）"变成"USB 大容量存储设备（无感叹号）"。

### 6.2 `iInterface = 4` 的字符串依赖

C1 阶段字符串描述符索引 4 不存在（C2 才补）。主机 GET_DESCRIPTOR(String 4) 会失败，但**不影响 Problem Code 消失**。主机对字符串描述符的容错较高，拿不到只会显示空字符串。

**验证**：C1 后 USBTreeView 的 `iInterface` 字段可能显示为空或"String Descriptor 4 not available"，这是正常的，C2 补全后消失。

### 6.3 `wTotalLength` 的字节序

`wTotalLength = 0x0020 (32)` 是 16 位小端序，在数组中存储为 `{0x20, 0x00}`。完成态的数组已按小端序排列（`MASS_SIZ_CONFIG_DESC` 宏展开为 `0x20`，后跟 `0x00`），直接沿用即可。

---

## 7. 验证流程

### 7.1 编译验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | Keil Build (F7) | 0 Error(s), 0 Warning(s) | 待验证 |

### 7.2 USBTreeView 验证（核心验收）

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 烧录，插 USB | 设备管理器出现"USB 大容量存储设备" | **C1 核心验收** |
| 2 | 设备管理器查看 | 无黄色感叹号 | **C1 核心验收** |
| 3 | USBTreeView 查看配置描述符 | Mass Storage Class, Bulk-Only, 2 Bulk EP, 自供电 | **C1 核心验收** |
| 4 | USBTreeView 查看 Used Endpoints | 3（EP0+EP1+EP2） | 待验证 |

### 7.3 静态检查

| 步骤 | 检查项 | 预期值 | 通过条件 |
|---|---|---|---|
| 1 | 配置描述符长度 | 32 字节 | ✅ 静态确认 |
| 2 | wTotalLength | 0x0020 (32) | ✅ 静态确认 |
| 3 | bInterfaceClass | 0x08 (Mass Storage) | ✅ 静态确认 |
| 4 | bInterfaceProtocol | 0x50 (Bulk-Only) | ✅ 静态确认 |
| 5 | 端点数量 | 2（EP1 IN + EP2 OUT） | ✅ 静态确认 |

---

## 8. 常见问题排查

### 8.1 编译报错：MASS_ConfigDescriptor 未定义

**原因**：`usb_desc.c` 未补 `MASS_ConfigDescriptor` 数组，或 `usb_prop.c` 引用了但未 include `usb_desc.h`。

**修复**：确认 `usb_desc.c` 已补数组（§5.2），`usb_prop.c` 第 9 行有 `#include "usb_desc.h"`（B2 已有）。

### 8.2 设备管理器仍显示黄色感叹号

**原因**：`Device_Property.GetConfigDescriptor` 仍为 0，或 `MASS_GetConfigDescriptor` 未正确返回 `Config_Descriptor` 指针。

**排查**：确认 `Device_Property` 的 `GetConfigDescriptor` 字段已改为 `MASS_GetConfigDescriptor`（§5.3 改动 3）。

### 8.3 USBTreeView 读不到配置描述符

**原因**：`Config_Descriptor` 的长度字段错误，或 `MASS_GetConfigDescriptor()` 返回 NULL。

**排查**：确认 `Config_Descriptor` 的第二个字段是 `MASS_SIZ_CONFIG_DESC`（32），`MASS_GetConfigDescriptor()` 调用 `Standard_GetDescriptorData(Length, &Config_Descriptor)`。

---

## 9. 与下一步的衔接

TR1-C1 完成后，黄色感叹号消失，设备从"未知 USB 设备"变成"USB 大容量存储设备"。下一步：

- **TR1-C2**：`usb_desc.c/h` 补入 4 组字符串描述符（语言 ID/厂商/产品/序列号），`hw_config.c` 实现 `Get_SerialNum()`，`main.c` 加 `Get_SerialNum()` 调用。USBTreeView 显示 "STMicroelectronics" / "STM32 W25Q32 Flash Disk" / 唯一 SN。
- **TR1-C3**：`usb_prop.c` 实现 `Mass_Storage_SetConfiguration()`（`bDeviceState = CONFIGURED` + 清 EP1/EP2 DTOG）。B4 的 `while (bDeviceState != CONFIGURED)` 退出。

> **C1 是 TR1-C 阶段最直观的一步**：黄色感叹号消失、设备类型识别、USBSTOR 驱动加载——这三个变化同时发生，学习者能明显感受到"配置描述符"的作用。

---

*文档结束。*
