# TR1-C5 详细设计：软件重连 USB_Cable_Config

## 修改记录

| 版本 | 日期 | 修改内容 | 修改人 |
|---|---|---|---|
| V1.0 | 2026-07-19 | 初始版本 | Copilot |

---

## 文档信息

| 项目 | 内容 |
|---|---|
| 需求编号 | TR1-C5 |
| 需求描述 | 软件重连 `USB_Cable_Config(DISABLE/ENABLE)`（PA12 Out_PP ↔ AF_PP 切换） |
| 验收标准 | 调用函数后 PC 重新枚举 |
| 所属阶段 | TR1-C — 枚举完整化（横向补全） |
| 前置文档 | `项目框架设计.md`、`TR1框架设计.md` |
| 前置代码 | TR1-C4 MSC 类请求已完成，USBSTOR 驱动加载 |
| 完成态参照 | `tmp\last_project\src\main.c`（完成态无 C5 演示代码） |

---

## 1. 需求分解

TR1-C5 是 TR1 阶段的最后一步，目标是验证软件重连能力——通过调用 `PowerOff()` + `PowerOn()` 让 PC 重新枚举设备，无需物理拔插 USB 线。

**C5 的特殊性**：它**不新增任何功能代码**。`USB_Cable_Config()` 在 A2 已实现，`PowerOff()`/`PowerOn()` 在 B3 已实现。C5 只需在 `main.c` 的主循环中加入一个**演示性的周期重连**，验证这些接口能在运行时工作。

**软件重连的原理**（文档 §6.1）：

本板无 PMOS 开关，R10 硬接到 3.3V（1.5kΩ 固定上拉）。软件重连通过 PA12 模式切换实现：

| 操作 | PA12 模式 | 效果 |
|---|---|---|
| `PowerOff()` → `USB_Cable_Config(DISABLE)` | Out_PP 输出低 | D+ 被拉低，主机认为设备断开 |
| `PowerOn()` → `USB_Cable_Config(ENABLE)` | AF_PP | D+ 上拉生效，主机认为设备插入 |

主机检测到 D+ 从低到高的跳变，发起重新枚举。

本任务只修改一个文件：

| 操作 | 文件 | 内容 |
|---|---|---|
| 修改 | `src/main.c` | 主循环加周期性 PowerOff → 延时 → PowerOn 演示 |

---

## 2. 关键技术决策

### 2.1 演示方式：周期性自动重连

在 `while(1)` 主循环中加入一个简单的延时计数器，每隔约 10 秒触发一次 `PowerOff()` → 延时 1 秒 → `PowerOn()`。

**为什么用周期性自动重连而非按钮触发**：
- 本项目无按钮 GPIO（C8T6 最小系统板通常无按键接 GPIO）
- 周期性重连无需额外硬件，烧录后自动演示
- 学习者能直观观察 PC 设备管理器的设备消失/重现

### 2.2 延时实现

不使用 SysTick 或定时器中断（避免与 USB 中争抢），用简单的 `for` 循环软件延时：

```c
static void Delay(__IO uint32_t nCount)
{
    for(; nCount != 0; nCount--);
}
```

**延时精度**：不精确，约几十毫秒到几秒级（取决于 `nCount` 和编译器优化）。C5 只需要"足够长让主机检测到断开"的延时，不需要精确计时。

### 2.3 重连时序

```
正常运行 (10秒)
  ↓
PowerOff()           — PA12 切 Out_PP 输出低, D+ 拉低
  ↓
Delay(约1秒)         — 等主机检测到断开
  ↓
PowerOn()            — PA12 切 AF_PP, D+ 上拉, CNTR 重新配置
  ↓
while (bDeviceState != CONFIGURED)  — 等待重新枚举完成
  ↓
正常运行 (10秒)       — 循环
```

**关键**：`PowerOn()` 后必须再次 `while (bDeviceState != CONFIGURED)` 等待重新枚举。`PowerOff()` 不重置 `bDeviceState`（它只设 `UNCONNECTED`），但 `PowerOn()` 后主机会重新发起 RESET → `MASS_Reset()` 设 `ATTACHED` → SET_CONFIGURATION → `Mass_Storage_SetConfiguration()` 设 `CONFIGURED`。

### 2.4 `bDeviceState` 的重置

`PowerOff()` 设置 `bDeviceState = UNCONNECTED`。`PowerOn()` 设置 `bDeviceState = ATTACHED`。后续 RESET → `MASS_Reset()` → `ATTACHED` → SET_CONFIGURATION → `CONFIGURED`。

**`while (bDeviceState != CONFIGURED)` 在 `PowerOn()` 后会阻塞**，直到重新枚举完成。这是正确的行为——确保下一轮重连前枚举已完成。

### 2.5 与完成态的差异

完成态（`last_project`）的 `main.c` **没有**周期性重连演示——它只在 `while(1)` 空循环。C5 的演示代码是学习用途的额外内容，不属于完成态。

**C5 验收后**：可保留演示代码（方便后续观察重连效果），也可改回 `while(1)` 空循环（与完成态一致）。建议保留，作为 TR1 阶段的可观测演示。

---

## 3. 涉及组件

```
┌─────────────────────────────────────────────────┐
│  main.c — 主循环                                 │
│                                                  │
│  while (1) {                                     │
│    Delay(10秒)        正常运行                   │
│    PowerOff()         PA12 → Out_PP, D+ 拉低     │
│    Delay(1秒)         等主机检测断开              │
│    PowerOn()          PA12 → AF_PP, D+ 上拉      │
│    while (bDeviceState != CONFIGURED)  等重新枚举 │
│  }                                               │
└─────────────────────────────────────────────────┘
```

---

## 4. 文件清单

### 4.1 修改文件

| 文件 | 修改内容 |
|---|---|
| `src/main.c` | 主循环加 `Delay()` 函数 + 周期性 PowerOff/PowerOn 演示 |

### 4.2 新建文件

无。

### 4.3 工程分组

无变化。

---

## 5. 接口设计

### 5.1 `src/main.c`

```c
#include "stm32f10x.h"
#include "hw_config.h"
#include "usb_lib.h"
#include "usb_pwr.h"

static void Delay(__IO uint32_t nCount)
{
    for(; nCount != 0; nCount--);
}

int main(void)
{
  Set_System();
  Set_USBClock();
  USB_Interrupts_Config();
  Get_SerialNum();
  USB_Init();
  PowerOn();

  while (bDeviceState != CONFIGURED);  /* 等待首次枚举完成 */

  while (1)
  {
    Delay(0x4FFFFF);                   /* 约 10 秒正常运行 */

    PowerOff();                        /* 软件断开: D+ 拉低 */
    Delay(0x0FFFFF);                   /* 约 2 秒, 等主机检测断开 */
    PowerOn();                         /* 软件重连: D+ 上拉 */

    while (bDeviceState != CONFIGURED); /* 等待重新枚举完成 */
  }
}
```

**变更说明**：
1. 加 `Delay()` 软件延时函数
2. 首次 `while (bDeviceState != CONFIGURED)` 保留（B4 已有）
3. 主循环改为：延时 10 秒 → `PowerOff()` → 延时 2 秒 → `PowerOn()` → 等待重新枚举

**延时参数**：`0x4FFFFF` 和 `0x0FFFFF` 是经验值，在 72MHz 下约 10 秒和 2 秒。实际时间取决于编译器优化，C5 只需"足够长"即可。

---

## 6. 实现要点与风险

### 6.1 C5 的可观测变化

**C4 → C5 的可观测变化**：

| 项 | C4 | C5 | 变化 |
|---|---|---|---|
| 设备管理器 | 设备持续存在 | **周期性消失/重现** | ✅ 软件重连演示 |
| USBTreeView | 持续连接 | **周期性断开/重连** | ✅ |

**C5 的验收**：烧录后观察设备管理器，约每 12 秒（10 秒运行 + 2 秒断开）设备消失一次，然后重新出现并完成枚举。

### 6.2 延时期间的 USB 中断

`Delay()` 是软件延时循环，期间 USB 中断仍能触发（中断不受主循环阻塞）。但 `PowerOff()` 后 D+ 拉低，主机不会发 USB 请求，中断不会触发。`PowerOn()` 后主机重新枚举，中断恢复。

### 6.3 `PowerOff()` 的完整性

`PowerOff()` 只做两件事：`USB_Cable_Config(DISABLE)` + `bDeviceState = UNCONNECTED`。它**不关闭 USB 外设时钟**，**不复位 USB 外设**。`PowerOn()` 会重新配置 CNTR（FRES → 0 → IMR_MSK），确保 USB 外设状态干净。

### 6.4 重连后的 `Get_SerialNum()`

`Get_SerialNum()` 只需在首次调用（`USB_Init()` 前）。重连时不需要再次调用——`MASS_StringSerial` 已在 RAM 中填充，`PowerOff()`/`PowerOn()` 不清除 RAM。

---

## 7. 验证流程

### 7.1 编译验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | Keil Build (F7) | 0 Error(s), 0 Warning(s) | 待验证 |

### 7.2 设备管理器验证（核心验收）

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 烧录，插 USB | 设备管理器出现"USB 大容量存储设备" | 待验证 |
| 2 | 等待约 10 秒 | 设备从设备管理器**消失** | **C5 核心验收** |
| 3 | 再等约 2 秒 | 设备**重新出现**并完成枚举 | **C5 核心验收** |
| 4 | 观察循环 | 每 12 秒重复消失/重现 | 待验证 |

### 7.3 USBTreeView 验证

| 步骤 | 操作 | 预期 | 通过条件 |
|---|---|---|---|
| 1 | 烧录后立即抓 USBTreeView | 设备连接，描述符完整 | 待验证 |
| 2 | 等 10 秒后抓 USBTreeView | 设备断开（Connection Status 变化） | 待验证 |
| 3 | 等 2 秒后抓 USBTreeView | 设备重新连接，描述符完整 | 待验证 |

---

## 8. 常见问题排查

### 8.1 设备管理器设备不消失

**原因**：`PowerOff()` 未执行，或 `USB_Cable_Config(DISABLE)` 未正确拉低 D+。

**排查**：
- 确认 `Delay(0x4FFFFF)` 的延时足够长（若太短，可能还没到 `PowerOff()` 就被观察到了）
- 确认 `PowerOff()` 调用了 `USB_Cable_Config(DISABLE)`
- 用万用表测 PA12 电压：`PowerOff()` 后应接近 0V

### 8.2 设备消失后不重现

**原因**：`PowerOn()` 未执行，或重新枚举失败。

**排查**：
- 确认 `Delay(0x0FFFFF)` 后有 `PowerOn()` 调用
- 确认 `while (bDeviceState != CONFIGURED)` 能退出（C3 的 `Mass_Storage_SetConfiguration` 已实现）
- 若不退出，可能重新枚举失败——检查 USBTreeView 的 Connection Status

### 8.3 重连后设备描述符错误

**原因**：`PowerOff()`/`PowerOn()` 之间 RAM 数据被破坏（极不可能）。

**排查**：`MASS_StringSerial` 在 RAM，`PowerOff()`/`PowerOn()` 不清除 RAM。若序列号变化，说明 RAM 被破坏。

---

## 9. TR1 阶段总结

TR1-C5 完成后，TR1 阶段全部 13 条任务（A1~A4 + B1~B4 + C1~C5）完成。

**TR1 验收清单对照**（文档 §4）：

| 验收需求 | 对应任务 | 状态 |
|---|---|---|
| TR1-01 设备管理器出现设备 | B2/B4 | ✅ |
| TR1-02 设备描述符正确 | B1 | ✅ |
| TR1-03 配置描述符正确 | C1 | ✅ |
| TR1-04 字符串描述符正确 | C2 | ✅ |
| TR1-05 支持软件重连 | C5 | ✅ |
| TR1-06 可响应 USB 标准请求 | C3 | ✅ |

**TR1 阶段产物**：
- USB 设备能被 PC 识别为"USB 大容量存储设备"
- 设备/配置/字符串描述符完整
- SET_ADDRESS/SET_CONFIGURATION 正确响应
- MSC 类请求（GET_MAX_LUN/BOT Reset）正确响应
- 软件重连可用
- USBSTOR 驱动加载（Problem Code 10 因 BOT 未实现，TR2 解决）

**进入 TR2**：TR1 完成后，EP1/EP2 的回调仍指向 `NOP_Process`。TR2 将接管这两个回调，实现 BOT 状态机和 SCSI 命令，让 PC 能读写存储介质。

---

*文档结束。*
