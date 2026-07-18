# Changelog

本文档记录项目的所有重要变更。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

## [0.3.1] - 2026-07-19
### Added
- 设备类型被 PC 正确识别：从"未知 USB 设备（设备描述符请求失败）"变为"USB 大容量存储设备"，Windows USBSTOR 驱动已关联——这是枚举流程从"失败"到"成功"的转折点
- 配置描述符完整可读：USBTreeView 可查看完整配置（Mass Storage Class, Bulk-Only Transport, SCSI transparent, 2 个 Bulk 端点, 自供电, 100mA）
- 新增设计文档《TR1-C1：配置描述符》

### 状态
- Keil 编译链接通过，0 Error 0 Warning
- USBTreeView 实测：Connection Status 从"枚举失败"变为"已连接"，设备描述符+配置描述符完整可读，USBSTOR.SYS 驱动已加载
- Problem Code 从 43（枚举失败）改善为 10（驱动启动失败）——USBSTOR 驱动初始化时发 SCSI 命令探测存储介质，但 BOT 协议尚未实现（TR2），设备无响应导致驱动启动失败。这是 TR1-C4（MSC 类请求）要解决的问题
- C1 验收通过（配置描述符完整，设备类型正确识别），可推进 TR1-C2

## [0.2.4] - 2026-07-19
### Added
- 最小枚举流程闭环：设备初始化序列（时钟→GPIO→NVIC→设备属性→D+上拉）完整串联，程序现在会等待主机完成 SET_CONFIGURATION 后才进入主循环，为枚举完成检测提供了明确的观测点
- 新增设计文档《TR1-B4：main 主流程串联》

### 状态
- Keil 编译链接通过，0 Error 0 Warning
- USBTreeView 实测：与 B2/B3 表现一致（设备描述符完整可读，Problem Code 43）——符合本阶段预期（主流程串联是内部行为，不影响主机端枚举过程）
- TR1-B 最小枚举阶段全部完成，可推进 TR1-C 逐层补全

## [0.2.3] - 2026-07-19
### Added
- USB 电源管理完善：D+ 上拉使能、USB 外设复位、中断使能现在通过正式的 PowerOn() 完成，替代了 A2 阶段的临时调用，USB 外设状态更干净，减少偶发性枚举失败
- USB 设备状态机激活：设备状态（UNCONNECTED→ATTACHED→POWERED→CONFIGURED）现在被正确跟踪，为后续枚举完成检测提供基础
- 挂起中断安全处理：主机总线空闲期触发的 SUSP 中断现在被安全忽略（不进入 STOP 模式），避免枚举被意外打断——这是枚举能稳定成功的关键保障
- 新增设计文档《TR1-B3：usb_pwr 电源管理与状态机》

### 状态
- Keil 编译链接通过，0 Error 0 Warning
- USBTreeView 实测：与 B2 表现一致（设备描述符完整可读，Problem Code 43）——符合本阶段预期（电源管理是内部改善，配置描述符待 C1 补全后 Problem Code 才消失）
- A4/B2 预留的恢复点全部激活：usb_istr.c 恢复 8 分支完整结构，usb_prop.c 的 bDeviceState 赋值已取消注释
- B3 验收通过，可推进 TR1-B4

## [0.2.2] - 2026-07-18
### Added
- PC 首次识别到设备：主机成功读取设备描述符（VID=0x0483 STMicroelectronics, PID=0x5720），设备管理器出现"未知 USB 设备"——TR1-01 验收通过，这是项目历史上第一次被 PC 识别
- USB 端点硬件配置完成：EP0 控制端点、EP1 Bulk IN、EP2 Bulk OUT 的类型与缓冲区地址已配置，主机已分配设备地址（0x29）
- 新增设计文档《TR1-B2：usb_prop 设备属性与 MASS_Reset》

### 状态
- Keil 编译链接通过，0 Error 0 Warning
- USBTreeView 实测：设备描述符 18 字节完整可读（VID/PID/bcdUSB/bMaxPacketSize0 正确），EP0 激活，主机分配设备地址——B2 核心验收通过
- 配置描述符与字符串描述符尚未实现（C1/C2），Windows 标记 Problem Code 43（设备启动失败）——预期行为，C1 补全后消失
- B2 验收通过，可推进 TR1-B3

## [0.2.1] - 2026-07-18
### Added
- 设备身份数据就位：USB 设备描述符（VID=0x0483 STMicroelectronics, PID=0x5720）已定义，为主机识别设备提供了数据基础
- 新增设计文档《TR1-B1：usb_desc 最小设备描述符》

### 状态
- Keil 编译链接通过，0 Error 0 Warning
- USBTreeView 实测：与 A2~A4 表现一致（主机识别 Full-Speed，设备描述符请求失败）——符合本阶段预期（仅提供数据，回调机制待 B2 接入）
- B1 验收通过，可推进 TR1-B2

## [0.1.4] - 2026-07-18
### Added
- USB 中断全链路打通：从 NVIC 中断通道（A2）到中断入口再到事件分发器，USB 外设产生的中断现在能被 CPU 捕获并分发到对应处理函数，骨架阶段（TR1-A）全部完成
- 新增设计文档《TR1-A4：usb_istr 中断服务与 ISTR 分发》

### 状态
- Keil 编译链接通过，0 Error 0 Warning
- USBTreeView 实测：与 A2/A3 表现一致（主机识别 Full-Speed，设备描述符请求失败）——符合本阶段预期（中断入口已就绪，但协议栈响应部分尚未实现）
- A4 验收通过，可推进 TR1-B 最小枚举

## [0.1.3] - 2026-07-18
### Added
- USB 端点缓冲区布局固定：EP0/EP1/EP2 在 USB 专用 RAM 中的收发位置已规划完成，采用无重叠的安全布局（规避了历史上因地址重叠导致枚举失败的问题）
- 端点传输回调占位：EP1~EP7 的传输完成回调已绑定到空操作，为后续 BOT 协议（TR2）接管 EP1/EP2 预留了替换点
- 新增设计文档《TR1-A3：usb_conf 端点与 PMA 配置》

### 状态
- Keil 编译链接通过，0 Error 0 Warning
- USBTreeView 实测：与 A2 表现一致（主机识别 Full-Speed，设备描述符请求失败）——符合本阶段预期（仅编译期配置，无运行行为变化）
- A3 验收通过，可推进 TR1-A4

## [0.1.2] - 2026-07-18
### Added
- USB 外设硬件初始化：插入 USB 后主机可识别到 Full-Speed 设备（D+ 上拉生效，USB 48MHz 时钟工作正常）
- USB 中断通道打通：USB 低优先级中断已在 NVIC 注册，为后续 ISTR 事件分发（TR1-A4）提供入口
- 软件重连能力（接口层）：提供通过软件控制 D+ 上拉通断的接口，后续可通过调用实现 PC 端设备拔出/重新插入的效果（TR1-C5 将启用）
- 新增设计文档《TR1-A2：hw_config 硬件初始化》

### 状态
- Keil 编译链接通过，0 Error 0 Warning
- USBTreeView 实测：主机识别 Full-Speed 设备，因协议栈未工作（无设备描述符响应）显示"设备描述符请求失败"——符合本阶段预期
- A2 验收通过，可推进 TR1-A3

## [0.1.1] - 2026-07-17
### Added
- 工程骨架就位：STM32F103C8 目标工程可编译、可链接、可烧录，为标准外设库与 USB 库后续集成提供基础
- 需求拆分落地：TR1 阶段拆分为"骨架→最小枚举→逐层补全"三层共 13 条开发任务，每条对应一次独立提交

### Fixed
- 修复 USB 库与应用层的三处硬耦合导致的编译/链接失败：补齐硬件配置头文件（USB 库编译依赖）、中断屏蔽配置（USB 库初始化依赖）、全局符号占位（USB 库链接依赖）

### 状态
- Keil 编译链接通过，0 Error 0 Warning，固件可烧录
- 发现并记录 ST USB 库与应用层的耦合点，为后续阶段扫清障碍
