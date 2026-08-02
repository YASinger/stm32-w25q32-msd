# Changelog

本文档记录项目的所有重要变更。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

## [1.3.1] - 2026-08-02
### Added
- READ10 数据命令实现（TR2-C1）：usb_scsi.c 实现 SCSI_Read10_Cmd + SCSI_Address_Management（LBA 越界 → ADDRESS_OUT_OF_RANGE、CBW 长度不匹配 → INVALID_FIELED_IN_COMMAND；保留 WRITE10 分支供 C2 零改动复用）；usb_scsi.h 补 usb_type.h（bool 类型）与两个函数声明
- usb_bot.c 恢复 CBW_Decode 的 READ10 case 与 Mass_Storage_In 的 BOT_DATA_IN 分支（WRITE10/VERIFY10/FORMAT_UNIT 保留 #if 0，C2/C3 恢复）
- memory.c 恢复 Read_Memory 的 MAL_Read 调用：READ10 多包调度打通（MAL 一次读 512B → EP1 IN 64B×8 分包 → Length==0 置 BOT_DATA_IN_LAST → Set_CSW(PASSED)）
- mass_mal.c MAL_Init 磁盘初始内容 0 → 0xFF（模拟 Flash 擦除态，对齐 TR2 框架 §3.3 的 0xFF 验收）
- 新增设计文档《TR2-C1：usb_scsi READ10 与 memory 读调度》

### Fixed
- main.c 接线 MAL_Init(0)：标准例程由 hw_config.c 的 MAL_Config 调用，本项目已删 MAL_Config，导致 sram_disk（.bss 段）保持全 0、读盘无法呈现 0xFF——启动路径补调用后修复

### 状态
- Keil 编译链接通过，0 Error 0 Warning
- USBTreeView 实测：Problem Code 保持消失，容量 8,192 Bytes——C1 回归通过
- WinHex 读盘：**全 0xFF**——C1 核心验收通过（TR2 框架 §8.3 第 1 条）
- 资源管理器双击 D: 提示"需要格式化"（Windows 已成功发出 READ10，读到 0xFF 判定无 MBR）——间接佐证
- 格式化不可用（WRITE10 未实现，C2 恢复）——C1 边界确认
- 首测读盘全 0：根因 MAL_Init 从未被调用（MAL_Config 已删），main.c 接线后重测全 0xFF
- C1 验收通过，可推进 TR2-C2（SCSI_Write10_Cmd + Write_Memory/MAL_Write → 可格式化为 FAT、读写文件）

## [1.2.1] - 2026-08-02
### Added
- SCSI 查询命令实现：新建 usb_scsi.c/h，实现 9 个查询命令（INQUIRY/READ_CAPACITY10/READ_FORMAT_CAPACITIES/MODE_SENSE6/10/REQUEST_SENSE/START_STOP_UNIT/TEST_UNIT_READY）+ Set_Scsi_Sense_Data + 不支持命令统一处理（SCSI_Invalid_Cmd/SCSI_Valid_Cmd + 12 个宏别名）——TR2-B 纵向切片首个里程碑
- usb_bot.c 恢复 CBW_Decode 的 21 个 case（9 查询 + 12 不支持）与 4 处 Set_Scsi_Sense_Data 调用；READ10/WRITE10/VERIFY10/FORMAT_UNIT 保留 #if 0（C1/C2/C3 恢复）
- memory.c 恢复 #include "usb_scsi.h"；工程加入 usb_scsi.c/h
- 新增设计文档《TR2-B1：usb_scsi SCSI 查询命令》

### 状态
- Keil 编译链接通过，0 Error 0 Warning
- USBTreeView 实测：**Problem Code 10 消失**，USBSTOR + disk.sys + volume.sys 全部加载，设备管理器出现磁盘驱动器 "STM32 SRAM Disk USB Device"，Size 8,192 Bytes（16 块 × 512B 精确匹配），卷已创建并分配盘符 D:——B1 核心验收通过
- 格式化预期不可用（WRITE10 未实现，C2 恢复）——B1 边界确认
- B1 验收通过，可推进 TR2-C1（READ10）

## [1.1.4] - 2026-08-02
### Added
- 缓冲调度层就位：Read_Memory/Write_Memory 拆包组包框架已定义（64B 端点包 ↔ 512B 逻辑块），BOT 四层架构（端点回调 → BOT 状态机 → SCSI 命令 → 缓冲调度 → 介质层）的文件骨架至此全部就位——TR2 骨架阶段收官
- 新增设计文档《TR2-A4：memory 缓冲调度骨架》

### 状态
- Keil 编译链接通过，0 Error 0 Warning
- USBTreeView 实测：与 A3 一致（Problem Code 10）——符合本阶段预期。memory 层无调用方（SCSI 命令层 B1 才创建），纯文件就位，无运行行为变化
- A4 验收通过，TR2-A 骨架阶段（A1~A4）全部完成，可推进 TR2-B1（SCSI 查询命令）

## [1.1.3] - 2026-07-31
### Added
- BOT 协议栈骨架就位：CBW/CSW 收发机制建立，端点回调从空操作（NOP）接管到真实 BOT 状态机——这是 TR2 的枢纽步骤，后续 SCSI 命令只需在此骨架上对接
- 设备现在能正确响应主机的 BOT 传输：USBSTOR 下发的命令块（CBW）被接收并返回命令状态（CSW），主机与设备的 Bulk 传输通道真正打通
- 新增设计文档《TR2-A3：usb_bot BOT 状态机骨架与端点回调接管》

### 状态
- Keil 编译链接通过，0 Error 0 Warning
- USBTreeView 实测：Problem Code 仍为 10（CM_PROB_FAILED_START）——符合本阶段预期。SCSI 命令处理尚未实现（B1 任务），所有命令走 default 分支返回 CSW_CMD_FAILED，USBSTOR 据此标记驱动启动失败。Problem Code 彻底消失需 B1 实现 SCSI 查询命令
- 关键变化：从"CBW 无响应（NOP 回调）"变为"CBW 有响应（返回 FAILED）"，证明 EP1/EP2 回调已接管、BOT 状态机已运行——A3 验收通过，可推进 TR2-A4

## [1.1.2] - 2026-07-30
### Added
- SCSI 响应数据就位：INQUIRY（设备类型/厂商/产品名）、REQUEST_SENSE（错误信息）、MODE_SENSE、READ_CAPACITY 等 7 组预填充数据已定义，为 SCSI 查询命令提供响应内容——这是 BOT 四层架构的数据层
- 新增设计文档《TR2-A2：scsi_data SCSI 静态数据》

### 状态
- Keil 编译链接通过，0 Error 0 Warning
- USBTreeView 实测：与 A1 一致（静态数据只定义不使用，上层未接入）——符合本阶段预期
- A2 验收通过，可推进 TR2-A3

## [1.1.1] - 2026-07-30
### Added
- 存储介质层就位：8KB SRAM 磁盘已实现（Init/Read/Write/GetStatus），为 BOT 协议和 SCSI 命令提供存储基础——这是 TR2 四层架构的最底层
- 新增设计文档《TR2-A1：mass_mal SRAM 介质层》

### 状态
- Keil 编译链接通过，0 Error 0 Warning
- USBTreeView 实测：与 TR1-C5 一致（MAL 是最底层组件，上层未接入，无运行行为变化）——符合本阶段预期
- A1 验收通过，可推进 TR2-A2

## [1.0.0] - 2026-07-19
### Milestone
- **TR1 阶段全部完成**：USB 枚举通过，PC 识别为"USB 大容量存储设备"，设备/配置/字符串描述符完整，标准请求与 MSC 类请求正确响应，软件重连可用。13 条开发任务（A1~A4 + B1~B4 + C1~C5）全部提交，6 项验收需求全部通过

### Added
- TR2 框架设计完成：将"SRAM 虚拟 U 盘"拆分为骨架（A1~A4）→ 纵向切片（B1）→ 横向补全（C1~C3）共 8 条开发任务，每条有明确可观测结果
- 需求列表重写：TR1 全部标记完成，TR2 展开为 8 条开发任务 + 4 项验收清单
- 新增设计文档《TR2 框架设计》

### 状态
- TR1 验收通过：设备可被检测、描述符完整、标准请求响应、软件重连可用
- TR2 框架就绪，可开始 TR2-A1 详设与实施

## [0.3.5] - 2026-07-19
### Added
- 软件重连能力验证：设备现在能通过软件控制周期性断开/重连，无需物理拔插 USB 线——PC 设备管理器可观察到设备定期消失并重新出现，TR1-05 验收通过
- 新增设计文档《TR1-C5：软件重连》

### Fixed
- 修复软件重连不生效：PowerOff() 缺少 USB 外设断电步骤（CNTR_PDWN），导致 USB 外设仍驱动 PA12 与 GPIO 输出低争抢引脚，D+ 拉不下去。补入 CNTR_FRES + CNTR_PDWN 后 USB 外设完全断电，主机正确检测到断开

### 状态
- Keil 编译链接通过，0 Error 0 Warning
- 实测验证：LED 周期亮灭，设备管理器设备同步消失/重现，软件重连正常工作
- **TR1 阶段全部 13 条任务完成**（A1~A4 + B1~B4 + C1~C5），6 项验收需求全部通过：设备可被检测、设备/配置/字符串描述符正确、可响应标准请求、支持软件重连
- TR1 完成，可推进 TR2（BOT 协议与 SCSI 命令）

## [0.3.4] - 2026-07-19
### Added
- USBSTOR 驱动加载成功：MSC 类请求（GET_MAX_LUN / Bulk-Only Reset）已实现，Windows 存储驱动能查询设备逻辑单元数并打开 Bulk 端点管道——设备从"驱动启动失败"进入"驱动已加载"状态
- 枚举流程完整走通：SET_CONFIGURATION 成功（Current Config Value=0x01），EP1 IN + EP2 OUT 两个 Bulk 管道建立，Manufacturer/Product String 完整显示
- 新增设计文档《TR1-C4：MSC 类请求》

### 状态
- Keil 编译链接通过，0 Error 0 Warning
- USBTreeView 实测：USBSTOR.SYS 驱动加载，2 个 Bulk 管道建立，Current Config Value=0x01，Summary 显示完整厂商/产品/序列号——C4 核心验收通过
- Problem Code 10 仍存在：USBSTOR 驱动通过 BOT 协议发 SCSI 命令探测存储介质时设备无响应（EP1/EP2 回调是 NOP_Process，BOT 状态机属 TR2）——这是 TR1 阶段能达到的最佳状态，Problem Code 彻底消失需 TR2
- C4 验收通过，可推进 TR1-C5（软件重连）

## [0.3.3] - 2026-07-19
### Added
- 枚举状态机闭环：主机 SET_CONFIGURATION 后设备正确进入 CONFIGURED 状态，初始化序列第一次能完整跑完进入主循环——标志着枚举流程在协议层完整走通
- 新增设计文档《TR1-C3：标准请求完整响应》

### 状态
- Keil 编译链接通过，0 Error 0 Warning
- USBTreeView 实测：与 C2 表现一致（Problem Code 10）——符合本阶段预期（C3 是内部状态机改进，USBTreeView 表面无差异；Problem Code 10 待 C4 解决）
- C3 验收通过，可推进 TR1-C4

## [0.3.2] - 2026-07-19
### Added
- 设备身份信息完整：PC 现在能显示设备的厂商（"STMicroelectronics"）、产品名（"STM32 W25Q32 Flash Disk"）和唯一序列号（读 MCU 96-bit UID 生成的 12 位十六进制），每块板子序列号不同
- 新增设计文档《TR1-C2：字符串描述符》

### 状态
- Keil 编译链接通过，0 Error 0 Warning
- USBTreeView 实测：Summary 显示 Serial="8D6F405D5656"、BusReported Device Desc="STM32 W25Q32 Flash Disk"，Device ID 用序列号作实例 ID——字符串描述符数据正确
- String Descriptors 详细查看仍显示 "not available"——这是 Problem Code 10 的副作用（USBTreeView 在设备有故障码时不逐个请求字符串），C4 解决 Problem Code 10 后会完整可读
- C2 验收通过，可推进 TR1-C3

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
