# stm32-w25q32-msd

基于 STM32F103C8T6 + W25Q32 的 USB 大容量存储设备（模拟 U 盘）。

## 硬件

| 组件 | 型号 |
|---|---|
| MCU | STM32F103C8T6 |
| Flash | W25Q32 (SPI, 4MB) |
| 接口 | USB Full-Speed |

## 构建

- IDE: Keil MDK V5
- 编译器: ARMCC V5.06
- 打开 `project.uvprojx` 编译即可

## 目录结构

```
├── inc/      — 头文件
├── src/      — 源文件
├── Start/    — 启动文件 + CMSIS
├── Library/  — 标准外设库 + USB 库
│   ├── STM32F10x_StdPeriph_Driver/
│   └── STM32_USB-FS-Device_Driver/
```

## 许可

MIT License.
