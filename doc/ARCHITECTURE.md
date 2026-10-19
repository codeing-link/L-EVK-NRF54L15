# 工程架构与模块来源

本文说明 `L-EVK-NRF54L15` 当前实际使用的软件分层、启动顺序、数据流，以及
`modules/lierda` 中各仓库的来源和集成边界。内容参考
[l-evk-t2-gh3220](https://gitee.com/XiaoPb/l-evk-t2-gh3220) 的 README，但以本工程
当前代码和最终构建配置为准。

## 1. 当前平台

| 项目 | 当前配置 |
| --- | --- |
| MCU | Nordic nRF54L15，CPUAPP |
| 自定义板目标 | `l_evk_nrf54l15/nrf54l15/cpuapp` |
| 板级目录 | `boards/lierda/l_evk_nrf54l15`，随应用一起管理 |
| 健康传感器 | Goodix GH3220，SPI21，4 MHz，Mode 0 |
| 加速度计 | LIS2DW12TR，TWIM20，100 kHz，地址 `0x19` |
| RTOS / SDK | Zephyr 4.4.2 / nRF Connect SDK v3.4.1 |
| 已验证编译器 | arm-zephyr-eabi-gcc 14.3 |
| 无线功能 | GH3220 GATT + MCUmgr SMP 共用 Zephyr Bluetooth 初始化 |
| 启动与升级 | MCUboot sysbuild，双槽 BLE OTA |

参考工程当前 README 基于 NCS v3.3.0 和独立 board 模块。本工程没有直接使用该 board
模块，而是维护自己的板级定义；参考工程中的 SPI20、I2C22、UART 管脚等描述也不能直接
套用到本工程。本工程的最终硬件定义以 `boards/lierda/l_evk_nrf54l15` 和编译生成的
`zephyr.dts` 为准。

## 2. 软件分层

```text
手机 / 上位机
  │
  ├─ GH3220 GATT：uprotocol 命令与数据
  └─ MCUmgr SMP：固件上传、镜像管理、复位
  │
  ▼
应用层 apps/L-EVK-NRF54L15
  ├─ main.c                    启动顺序、LED、日志等级、复位原因
  ├─ imu_lis2dw12.c            LIS2DW12 初始化与周期采样
  ├─ gh3220_ble_service.c      统一 BLE 初始化、广播和异步收发
  └─ 引用 l-evk-t2-gh3220/app_cardiff_a/src
       ├─ gh3220_framework.c   SDK 生命周期与健康框架接入
       ├─ gh3220_scheduler.c   1000 ms RTC tick
       ├─ gh3220_protocol_user.c / gh3220_frame.c
       └─ gh3220_sdk/*         产品寄存器表与应用钩子
  │
  ▼
core_framework
  应用线程、消息队列、健康/IMU/PHAS 事件分发
  │
  ▼
os_adapter
  任务、队列、信号量、互斥量、定时器的 Zephyr OSAL 实现
  │
  ▼
gh3220_sdk
  GH3220 驱动、FIFO、算法调度、HR/HRV/SpO2/ECG/NADT、协议
  │
  ▼
本工程自定义 board + Zephyr/NCS
  SPI21、TWIM20、GPIO、UART30、Bluetooth、MCUboot、MCUmgr
  │
  ▼
L-EVK-NRF54L15 硬件
  GH3220 + LIS2DW12TR + LED + UART
```

本工程只在应用仓库和自定义 board 中维护自己的改动。`modules/lierda` 保持供应商仓库
原样，便于后续使用更新脚本跟随上游版本。

## 3. 启动顺序

`main.c` 的主要顺序如下：

1. 为应用、IMU 和 GH3220 Zephyr 日志模块设置统一运行时等级。
2. 读取、打印并清除本次复位原因。
3. 初始化两路 LED 和 LIS2DW12TR。
4. 设置 1000 ms framework tick，启动 `core_framework` 应用线程。
5. 等待 GH3220 SDK 初始化完成，再启动 RTC scheduler。
6. 初始化一次 Bluetooth，同时注册 GH3220 GATT 与 MCUmgr SMP 服务并开始广播。
7. 主循环每秒翻转 LED、采集一次 IMU；周期 LED/ACC 信息只在 DBG 等级输出。

GH3220 使用 `CONFIG_GH3220_RTC_POLLING=y`。定时器只触发 framework 消息，实际 SDK、
SPI 和算法处理在消息队列线程中执行，不在定时器回调中执行阻塞式传感器访问。

## 4. 主要数据流

### 4.1 GH3220 采集与算法

```text
1000 ms RTC tick
  → MQ_EVENT_HEALTH_INT_UPDATE
  → Gh3x2xDemoInterruptProcess()
  → SPI21 读取 GH3220 FIFO
  → SDK 算法调度
  → HR / HRV / SpO2 / ECG / NADT 结果钩子
  → uprotocol / BLE 上报
```

SPI21 使用 P1.12 SCK、P1.13 MOSI、P1.10 MISO；P1.11 是 SDK 软件控制的低有效
CS，P1.09 是低有效 RESET，P1.14 是高有效 INT。完整管脚定义见
[传感器接口说明](SENSORS.md)。

### 4.2 BLE 上下行

```text
手机写入 GATT
  → gh3220_ble_service RX 队列
  → gh3220_protocol_receive()
  → gh3220_frame_feed() 重组 uprotocol 帧
  → Gh3x2xDemoProtocolProcess()

SDK 上报
  → Gh3x2xUserSerialSendData()
  → gh3220_ble_notify() TX 队列
  → GATT Notify
```

RX/TX 在独立线程和消息队列中处理，避免 Bluetooth 回调直接执行耗时 SDK 操作。连接、
断开或通知订阅变化时会清理旧会话状态，避免上一连接的残留数据被新连接继续处理。

### 4.3 BLE OTA

GH3220 GATT 与 MCUmgr SMP 共用一次 Bluetooth Host/Controller 初始化。应用广播同时携带
SMP 服务信息和 GH3220 服务信息；MCUmgr 将新镜像写入 image-1，MCUboot 在复位后验证、
试运行并支持未确认镜像回滚。详细流程见 [Bluetooth LE OTA](BLE_OTA.md)。

## 5. 上游仓库与实际使用状态

这些目录是相互独立的 Git 仓库，不是应用仓库的 Git submodule。它们统一放在
`<ncs>/modules/lierda`，由本工程脚本维护。

| 仓库 | `west.yml` revision | 本工程用途 | 构建状态 |
| --- | --- | --- | --- |
| [l-evk-t2-gh3220](https://gitee.com/XiaoPb/l-evk-t2-gh3220) | 上游默认分支 | 参考应用；直接引用 `app_cardiff_a/src` 中 framework、scheduler、协议、frame、寄存器表和钩子源码 | 部分源码直接编译进应用 |
| [gh3220_sdk](https://gitee.com/XiaoPb/gh3220_sdk) | `master` | GH3220 驱动、Zephyr port、算法源码/静态库和协议 | 作为 Zephyr extra module 编译 |
| [os_adapter](https://gitee.com/XiaoPb/os_adapter/tree/dev-zephyr) | `dev-zephyr` | Zephyr OSAL 实现 | 作为 Zephyr extra module 编译 |
| [core_framework](https://gitee.com/XiaoPb/core_framework/tree/dev-zephyr) | `dev-zephyr` | 应用线程、消息队列、健康/IMU/PHAS 框架 | 作为 Zephyr extra module 编译 |
| [factory](https://gitee.com/XiaoPb/factory/tree/dev-zephyr) | `dev-zephyr` | 供应商工厂测试功能，预留后续使用 | 会同步，但当前不加入构建 |
| [l-evk-t2-nrf54l15](https://gitee.com/XiaoPb/l-evk-t2-nrf54l15) | `master` | 参考工程的独立板级模块 | 明确跳过；由本工程自建 board 取代 |

实际分支不在更新脚本中写死。脚本先更新 `l-evk-t2-gh3220` 的远端默认分支，再读取它
最新的 `west.yml`，因此以后子仓库分支或 revision 发生变化时会按 manifest 自动切换。

## 6. 模块更新和版本锁定

更新命令：

```bash
cd <ncs-workspace>/ncs/apps/L-EVK-NRF54L15
./update-modules.sh
```

更新脚本会执行以下操作：

1. 检查已有供应商仓库是否干净；存在本地修改时拒绝覆盖。
2. 更新 `l-evk-t2-gh3220` 的远端默认分支。
3. 解析该仓库最新 `west.yml` 中位于 `modules/lierda` 的项目、URL 和 revision。
4. 快进更新对应分支；revision 如果不是分支则使用 detached HEAD。
5. 跳过 `l-evk-t2-nrf54l15`，保留本工程自己的 board。
6. 生成 `modules.lock.yml`，记录每个仓库实际 URL、revision 和 commit。

更新完成后应检查 `modules.lock.yml` 的变化并执行：

```bash
PRISTINE=always ./build.sh
```

`modules.lock.yml` 应与应用代码一起提交，以便问题复现和版本追踪。不要直接修改
`modules/lierda` 中的供应商代码；如果必须临时调试，应先独立记录补丁并避免随后运行更新
脚本覆盖未提交工作。

## 7. 日志与诊断

工程使用 Zephyr deferred logging：

```text
日志缓冲区       8192 bytes
日志线程栈       1536 bytes
日志处理间隔     50 ms
默认应用等级     LOG_LEVEL_INF
```

`src/main.c` 的 `L_EVK_LOG_LEVEL` 统一控制 `l_evk_blinky`、`l_evk_imu`、
`gh3220_port`、`gh3220_raw_data` 和 `gh3220_ble`。默认不会输出 LED 状态和 ACC 周期样本；
改为 `LOG_LEVEL_DBG` 后才输出。启动时的 reset-cause 日志用于区分上电、外部引脚、软件、
看门狗、brownout、CPU lockup 或 bootloader 等复位来源。

## 8. 构建边界

`CMakeLists.txt` 只把 `gh3220_sdk`、`os_adapter` 和 `core_framework` 注册为
`ZEPHYR_EXTRA_MODULES`，同时显式编译参考应用所需源码。`factory` 不参与编译，参考 board
模块也不参与 board discovery。若上游目录结构或源文件名发生变化，CMake 会在配置阶段
报告缺失模块或缺失参考源码；更新模块后应始终执行 pristine build。

