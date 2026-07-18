# Changelog

本文档记录项目的所有重要变更。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

## [0.1.1] - 2026-07-17
### Added
- TR1-A1：工程骨架与库集成
- `project.uvprojx` 配置 4 个 Group（src / Start / StdPeriph / USB-FS-Device），STM32F103C8 设备选型，`USE_STDPERIPH_DRIVER` 宏，4 条 IncludePath
- `inc/usb_conf.h` 占位文件（满足 `usb_type.h` 的 `#include` 依赖，正式内容由 TR1-A3 填充）
- 设计文档 `docs/TR1框架设计.md`、`docs/详设/TR1-A1：工程骨架与库集成.md`
- 需求列表更新为"骨架→最小枚举→逐层补全"三层拆分结构（TR1-A/B/C 共 13 条开发任务）

### Fixed
- 补 `inc/hw_config.h` 占位：ST USB 库 `usb_lib.h:44` 硬性 `#include "hw_config.h"`，A1 未提供导致 6 个 USB 库 `.c` 编译失败
- `inc/hw_config.h` 加入 `#include "stm32f10x.h"`：`usb_regs.h` 直接使用 `__IO`/`uint16_t` 等 CMSIS 类型却不自包含，依赖 `hw_config.h` 作为类型入口，缺失导致 180 个编译错误
- `inc/usb_conf.h` 补 `IMR_MSK` 定义：`usb_sil.c:73` (`USB_SIL_Init`) 硬性引用该宏，掩码项遵循 §6.2 禁用挂起模式约束
- 新增 `src/usb_globals.c`：用 `__attribute__((weak))` 提供 6 个 USB 库链接依赖的全局符号占位（`wIstr`/`pEpInt_IN`/`pEpInt_OUT`/`Device_Table`/`Device_Property`/`User_Standard_Requests`），这些符号按设计属 A4/B2，但链接器在 A1 即需引用

### 状态
- Keil 编译链接通过，0 Error 0 Warning，`Objects/project.axf` 生成
- 发现并记录 ST USB 库三处与应用层的硬耦合点（详见 `docs/详设/TR1-A1：工程骨架与库集成.md` §8）
