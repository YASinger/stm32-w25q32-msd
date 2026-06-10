# Changelog

本文档记录项目的所有重要变更。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

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
