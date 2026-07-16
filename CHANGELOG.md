# Changelog

本文档记录项目的所有重要变更。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

## [0.2.2] - 2026-07-15
### Fixed
- 修复设备管理器黄色感叹号（CM_PROB_FAILED_START）：MSC 类请求（GET_MAX_LUN、Bulk-Only Mass Storage Reset）此前被 STALL，导致 Windows USBSTOR 驱动启动失败。现已正确响应
- 修复产品字符串内容与需求不符：由 "STM32 Mass Storage" 改为 "STM32 W25Q32 Flash Disk"

### Added
- 新增序列号唯一性：读取 MCU 唯一 ID 生成 12 位十六进制序列号，每块板子不同
- 实现 GET_MAX_LUN 类请求响应（返回 1 字节，表示仅 1 个 LUN）
- 实现 Bulk-Only Mass Storage Reset 类请求响应（复位 Bulk 端点）
- 设备属性回调表与标准请求回调表添加中文注释


## [0.2.1] - 2026-07-03
### Fixed
- 修复 USB 枚举失败根因：`fSuspendEnabled` 由 `TRUE` 改为 `FALSE`，避免 SUSP 中断将 MCU 拉入不可唤醒的 STOP 模式（USBWakeUp 中断未配置，唤醒路径缺失）
- 修正 `usb_conf.h` PMA 端点缓冲区地址为 16-bit 字偏移（ENDP0_RXADDR 0x18→0x20 等，原字节偏移导致与 BTABLE 重叠）

### Added
- `main.c` 添加 PC13 LED 初始化及枚举成功指示（CONFIGURED 后点亮）

### Changed
- `main.c` 显式调用 `PowerOn()`（原由 `USB_Init` 内部调用）
- `hw_config.c` USBWakeUp 中断由 `ENABLE` 改为 `DISABLE`（TR1 阶段不使用挂起/唤醒）

## [0.2.0] - 2026-06-10
### Added
- 实现 TR1-01 USB 设备可被主机检测到
- `hw_config.h/c` 硬件初始化（时钟 / GPIO / NVIC / USB_Cable_Config）
- `usb_desc.h/c` USB 描述符（设备 / 配置 / 5 个字符串）
- `usb_pwr.h/c` 电源管理（PowerOn / PowerOff / Suspend / Resume）
- `usb_prop.c` Device_Property + User_Standard_Requests 全部回调
- `usb_istr.c` USB_Istr() ISTR 事件分发 + USB_LP_CAN1_RX0_IRQHandler
- `usb_conf.h` EP_NUM / BTABLE_ADDRESS / 端点缓冲区地址 / CTR 回调宏
- 设计文档 项目框架设计.md / 需求列表.md / TR1-01：USB 设备可被主机检测到.md

### Changed
- `main.c` 由空骨架改为完整 USB 枚举主流程

## [0.1.0] - 2026-06-09
### Added
- 初始化 STM32F103C8T6 工程骨架
- 集成 STM32 标准外设库 V3.5.0
- 集成 STM32 USB-FS-Device 库 V4.1.0
- 添加 USB 配置头文件（hw_config.h / usb_conf.h）及链接桩
- 配置 Keil MDK 编译环境（ARMCC V5.06）
- 目标功能：W25Q32 SPI Flash 模拟 USB 大容量存储设备
