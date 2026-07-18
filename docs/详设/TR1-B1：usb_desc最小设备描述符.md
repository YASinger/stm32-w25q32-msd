# TR1-B1 详细设计：usb_desc 最小设备描述符

## 修改记录

| 版本 | 日期 | 修改内容 | 修改人 |
|---|---|---|---|
| V1.0 | 2026-07-18 | 初始版本 | Copilot |

---

## 文档信息

| 项目 | 内容 |
|---|---|
| 需求编号 | TR1-B1 |
| 需求描述 | `usb_desc.c/h` 最小设备描述符（18B，VID=0x0483 PID=0x5720 bcdUSB=2.00 bMaxPacketSize0=64） |
| 验收标准 | 主机 GET_DESCRIPTOR(Device) 能拿到 18B 合法数据 |
| 所属阶段 | TR1-B — 最小枚举通过（纵向切片） |
| 前置文档 | `项目框架设计.md`、`TR1框架设计.md` |
| 前置代码 | TR1-A 骨架全部完成（A1~A4） |
| 完成态参照 | `tmp\last_project\src\usb_desc.c`、`tmp\last_project\inc\usb_desc.h`（TR1 全部完成后的最终形态） |

---

## 1. 需求分解

TR1-B1 是 TR1-B 最小枚举的第一步，目标是让主机发出的 `GET_DESCRIPTOR(Device)` 请求能拿到 18 字节合法设备描述符。这是 PC 设备管理器能识别到设备的**数据基础**——没有设备描述符，主机连 VID/PID 都读不到（A2/A3/A4 阶段的"设备描述符请求失败"就是因为这个）。

**与完成态的关系**：`last_project` 的 `usb_desc.c/h` 包含全部 6 组描述符（设备/配置/4 组字符串），是 TR1-C1/C2 完成后的最终形态。B1 只做**最小设备描述符**——只定义 `MASS_DeviceDescriptor` 数组和 `MASS_SIZ_DEVICE_DESC` 宏，其余留待 C1/C2 补全。

本任务需要新建两个文件：

| 操作 | 文件 | 内容 |
|---|---|---|
| 新建 | `src/usb_desc.c` | `MASS_DeviceDescriptor` 数组（18B） |
| 新建 | `inc/usb_desc.h` | `MASS_SIZ_DEVICE_DESC` 宏 + `MASS_DeviceDescriptor` 声明 |

**关键认知**：B1 只提供**数据**（设备描述符数组），**没有机制**把它返回给主机。真正把描述符发给主机的是 B2 的 `MASS_GetDeviceDescriptor()` 回调（经 `usb_prop.c` 的 `Device_Property` 结构体注册到 USB 库）。所以 B1 完成后插 USB，PC 端表现与 A2~A4 一致（"设备描述符请求失败"）——因为 B2 的回调还没接上去。

---

## 2. 关键技术决策

### 2.1 设备描述符字段（§6.7）

设备描述符 18 字节的每个字段都按文档 §6.7 的明确值定义：

| 字段 | 值 | 说明 |
|---|---|---|
| bLength | 0x12 (18) | 描述符长度 |
| bDescriptorType | 0x01 | DEVICE |
| bcdUSB | 0x0200 | USB 2.0 |
| bDeviceClass | 0x00 | 类在接口级定义（MSC 在配置描述符的接口描述符中定义） |
| bDeviceSubClass | 0x00 | — |
| bDeviceProtocol | 0x00 | — |
| bMaxPacketSize0 | 0x40 (64) | EP0 最大包大小 |
| idVendor | 0x0483 | STMicroelectronics |
| idProduct | 0x5720 | 自定义 PID |
| bcdDevice | 0x0200 | 设备版本 2.00 |
| iManufacturer | 0x01 | 字符串描述符索引 1 |
| iProduct | 0x02 | 字符串描述符索引 2 |
| iSerialNumber | 0x03 | 字符串描述符索引 3 |
| bNumConfigurations | 0x01 | 1 个配置 |

**注意**：`iManufacturer`/`iProduct`/`iSerialNumber` 指向字符串描述符索引 1/2/3，但 B1 阶段字符串描述符尚未定义（C2 才做）。主机读到索引后会尝试 GET_DESCRIPTOR(String)，若 B2/C2 未完成会返回 STALL——这是 B1 阶段的预期行为，不影响设备描述符本身的合法性。

### 2.2 与完成态的裁剪关系

完成态 `usb_desc.h` 定义了 6 组描述符的宏和声明，B1 只保留设备描述符相关部分：

| 完成态内容 | B1 是否保留 | 恢复时机 |
|---|---|---|
| `MASS_SIZ_DEVICE_DESC` + `MASS_DeviceDescriptor` | ✅ 保留 | — |
| `MASS_SIZ_CONFIG_DESC` + `MASS_ConfigDescriptor` | ❌ 删除 | C1 |
| `MASS_SIZ_STRING_LANGID` + `MASS_StringLangID` | ❌ 删除 | C2 |
| `MASS_SIZ_STRING_VENDOR` + `MASS_StringVendor` | ❌ 删除 | C2 |
| `MASS_SIZ_STRING_PRODUCT` + `MASS_StringProduct` | ❌ 删除 | C2 |
| `MASS_SIZ_STRING_SERIAL` + `MASS_StringSerial` | ❌ 删除 | C2 |
| `MASS_SIZ_STRING_INTERFACE` + `MASS_StringInterface` | ❌ 删除 | C2 |

完成态的 `usb_desc.c` 同样只保留 `MASS_DeviceDescriptor` 数组，其余 5 组数组删除。

### 2.3 `MASS_StringSerial` 的 `const` 属性

完成态中 `MASS_StringSerial` 是 `uint8_t`（非 const），因为 C2 的 `Get_SerialNum()` 要在运行时修改它（读 MCU UID 填充序列号）。B1 阶段不涉及字符串描述符，这个差异不影响。C2 恢复时保持非 const。

---

## 3. 涉及组件

```
┌─────────────────────────────────────────────────┐
│  主机 GET_DESCRIPTOR(Device) 请求                │
└────────────────────┬────────────────────────────┘
                     │ EP0 控制传输
╔════════════════════╧═════════════════════════════╗
║  desc (USB 设备层)                                ║
║  ┌───────────────────────────────────────────┐  ║
║  │ usb_desc.c / usb_desc.h                   │  ║
║  │  ├─ MASS_DeviceDescriptor[18]  设备描述符 │  ║
║  │  └─ MASS_SIZ_DEVICE_DESC       长度宏 = 18│  ║
║  └───────────────────────────────────────────┘  ║
╠══════════════════════════════════════════════════╣
║  被引用 (B2 接入)                                ║
║  ┌──────────────────────────────────────────┐   ║
║  │ usb_prop.c (B2)                          │   ║
║  │  MASS_GetDeviceDescriptor()              │   ║
║  │   → 返回 MASS_DeviceDescriptor 指针       │   ║
║  └──────────────────────────────────────────┘   ║
╚══════════════════════════════════════════════════╝
```

**依赖关系**：
- `usb_desc.c` 依赖 `usb_desc.h` 的宏定义（`MASS_SIZ_DEVICE_DESC`）
- `usb_desc.h` 依赖 `stm32f10x.h` 的 `uint8_t` 类型（经 `hw_config.h` 已由 USB 库间接引入）
- B2 的 `MASS_GetDeviceDescriptor()` 将引用 `MASS_DeviceDescriptor` 和 `MASS_SIZ_DEVICE_DESC`

---

## 4. 文件清单

### 4.1 新建文件

| 文件 | 说明 |
|---|---|
| `src/usb_desc.c` | `MASS_DeviceDescriptor` 数组（18B） |
| `inc/usb_desc.h` | `MASS_SIZ_DEVICE_DESC` 宏 + `MASS_DeviceDescriptor` 声明 |

### 4.2 修改文件

无。

### 4.3 工程分组

`project.uvprojx` 的 `src` Group 加入 `usb_desc.c` 和 `usb_desc.h`。

---

## 5. 接口设计

### 5.1 `inc/usb_desc.h`

```c
#ifndef __USB_DESC_H
#define __USB_DESC_H

#include "stm32f10x.h"

#define MASS_SIZ_DEVICE_DESC              18

extern const uint8_t MASS_DeviceDescriptor[MASS_SIZ_DEVICE_DESC];

#endif /* __USB_DESC_H */
```

**裁剪说明**：完成态定义了 6 组描述符的宏和声明，B1 只保留设备描述符。C1 恢复配置描述符，C2 恢复 4 组字符串描述符。

### 5.2 `src/usb_desc.c`

```c
#include "usb_desc.h"

/* ── 设备描述符 (18 字节) ─────────────────────────────────────────────────── */
const uint8_t MASS_DeviceDescriptor[MASS_SIZ_DEVICE_DESC] = {
    0x12,   /* bLength           = 18                            */
    0x01,   /* bDescriptorType   = DEVICE                        */
    0x00,   /* bcdUSB, version 2.00                              */
    0x02,
    0x00,   /* bDeviceClass    = 0 (接口级别定义)                 */
    0x00,   /* bDeviceSubClass                                    */
    0x00,   /* bDeviceProtocol                                    */
    0x40,   /* bMaxPacketSize0 = 64                              */
    0x83,   /* idVendor        = 0x0483 (STMicroelectronics)     */
    0x04,
    0x20,   /* idProduct       = 0x5720                          */
    0x57,
    0x00,   /* bcdDevice       = 2.00                            */
    0x02,
    1,      /* iManufacturer                                     */
    2,      /* iProduct                                          */
    3,      /* iSerialNumber                                     */
    0x01    /* bNumConfigurations                                */
};
```

**说明**：与完成态完全一致，只保留 `MASS_DeviceDescriptor` 数组，其余 5 组数组（配置/语言 ID/厂商/产品/序列号/接口）删除。

---

## 6. 实现要点与风险

### 6.1 B1 的可观测性（最重要）

**问题**：B1 只提供数据（设备描述符数组），没有机制把它返回给主机。插 USB 后 PC 端表现与 A2~A4 一致（"设备描述符请求失败"）。

**原因**：主机发 `GET_DESCRIPTOR(Device)` 后，USB 库的 `usb_core.c` 调用 `Device_Property.GetDeviceDescriptor()`，而 `Device_Property` 是 `usb_globals.c` 中的 weak 占位（全零结构体），`GetDeviceDescriptor` 成员是 NULL 指针。调用 NULL 函数指针会 HardFault——但实际上不会走到这一步，因为 `wInterrupt_Mask` 仍为 0（`USB_SIL_Init()` 未被调用），CTR 分支不进，EP0 的 SETUP 包根本不会被处理。

**结论**：B1 的验收标准"主机 GET_DESCRIPTOR(Device) 能拿到 18B 合法数据"**无法在 B1 阶段直接验证**——它需要 B2 的回调机制配合。B1 的验收只能通过**间接方式**：
1. 编译通过（语法正确）
2. 静态检查数组内容（与文档 §6.7 对照）
3. B2 完成后，USBTreeView 读到 VID=0x0483 PID=0x5720，回头证明 B1 的数据正确

**风险等级**：低。只要数组内容与文档 §6.7 一致，B2 接入后自然工作。

### 6.2 字节序

USB 描述符的多字节字段（`bcdUSB`/`idVendor`/`idProduct`/`bcdDevice`）是**小端序**（低字节在前）。完成态的数组已按小端序排列：
- `bcdUSB = 0x0200` → `{0x00, 0x02}`
- `idVendor = 0x0483` → `{0x83, 0x04}`
- `idProduct = 0x5720` → `{0x20, 0x57}`
- `bcdDevice = 0x0200` → `{0x00, 0x02}`

直接沿用完成态的值即可，无需调整。

### 6.3 `const` 属性

`MASS_DeviceDescriptor` 是 `const uint8_t`，存储在 Flash 而非 RAM。设备描述符在运行时不变，const 正确。C2 的 `MASS_StringSerial` 是非 const（运行时由 `Get_SerialNum()` 修改），B1 不涉及。

---

## 7. 验证流程

### 7.1 编译验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | Keil Build (F7) | 0 Error(s), 0 Warning(s) | 待验证 |

### 7.2 静态检查

| 步骤 | 检查项 | 预期值 | 通过条件 |
|---|---|---|---|
| 1 | 数组长度 | 18 字节 | ✅ 静态确认 |
| 2 | bLength | 0x12 | ✅ 静态确认 |
| 3 | idVendor | 0x83, 0x04（小端 = 0x0483） | ✅ 静态确认 |
| 4 | idProduct | 0x20, 0x57（小端 = 0x5720） | ✅ 静态确认 |
| 5 | bcdUSB | 0x00, 0x02（小端 = 0x0200） | ✅ 静态确认 |
| 6 | bMaxPacketSize0 | 0x40 (64) | ✅ 静态确认 |

### 7.3 USBTreeView 验证（预期无变化）

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 烧录，插 USB | 与 A2~A4 相同："设备描述符请求失败" | 待验证 |

> B1 只提供数据，B2 的回调机制未接入。USBTreeView 表现应与 A2~A4 完全一致。

### 7.4 B2 完成后的回归验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | B2 完成后烧录，插 USB | USBTreeView 读到 VID=0x0483 PID=0x5720 | 待 B2 后验证 |

> 这是 B1 数据的真正验收。B2 完成后若 USBTreeView 能读到正确的 VID/PID，证明 B1 的设备描述符数据正确。

---

## 8. 常见问题排查

### 8.1 编译报错：undefined symbol MASS_DeviceDescriptor

**原因**：`usb_desc.c` 未加入 uvprojx 的 src Group，Keil 未编译它。

**修复**：确认 `project.uvprojx` 的 src Group 已加入 `usb_desc.c`（§4.3）。

### 8.2 编译报错：MASS_SIZ_DEVICE_DESC 未定义

**原因**：`usb_desc.h` 未定义该宏，或 `usb_desc.c` 未 `#include "usb_desc.h"`。

**修复**：确认 `usb_desc.h` 中有 `#define MASS_SIZ_DEVICE_DESC 18`，`usb_desc.c` 第一行有 `#include "usb_desc.h"`。

### 8.3 B2 完成后 USBTreeView 仍读不到 VID/PID

**原因**：可能是 B1 的设备描述符数据错误，或 B2 的回调未正确返回数组指针。

**排查**：
1. 对照文档 §6.7 静态检查数组内容
2. 确认 B2 的 `MASS_GetDeviceDescriptor()` 返回 `MASS_DeviceDescriptor` 指针
3. 确认 B2 的 `Device_Descriptor` 结构体包含正确的长度 `MASS_SIZ_DEVICE_DESC`

---

## 9. 与下一步的衔接

TR1-B1 完成后，设备描述符数据就位。下一步：

- **TR1-B2**：新建 `src/usb_prop.c/h`，定义 `Device_Table`/`Device_Property`/`User_Standard_Requests`（覆盖 `usb_globals.c` 剩余 3 个 weak 占位），实现 `MASS_Reset`/`MASS_GetDeviceDescriptor` 等回调。B2 会引用 B1 的 `MASS_DeviceDescriptor` 和 `MASS_SIZ_DEVICE_DESC`。B2 完成后 `usb_globals.c` 可整个移除，设备管理器出现 "Unknown Device"——这是 TR1-01 的真正含义。
- **TR1-C1**：在 `usb_desc.c/h` 中补入 `MASS_ConfigDescriptor`（32B 配置描述符），USBTreeView 显示 Mass Storage Class + 2 Bulk EP。
- **TR1-C2**：在 `usb_desc.c/h` 中补入 4 组字符串描述符，`hw_config.c` 中实现 `Get_SerialNum()`，USBTreeView 显示厂商/产品/序列号。

> **B2 依赖 B1**：B2 的 `MASS_GetDeviceDescriptor()` 需要返回 `MASS_DeviceDescriptor` 指针，`Device_Descriptor` 结构体需要 `MASS_SIZ_DEVICE_DESC` 长度。若 B1 未完成，B2 编译失败。

---

*文档结束。*
