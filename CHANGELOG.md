# Changelog

本文档记录项目的所有重要变更。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

## [0.1.0] - 2026-06-09
### Added
- 初始化 STM32F103C8T6 工程骨架
- 集成 STM32 标准外设库 V3.5.0
- 集成 STM32 USB-FS-Device 库 V4.1.0
- 添加 USB 配置头文件（hw_config.h / usb_conf.h）及链接桩
- 配置 Keil MDK 编译环境（ARMCC V5.06）
- 目标功能：W25Q32 SPI Flash 模拟 USB 大容量存储设备
