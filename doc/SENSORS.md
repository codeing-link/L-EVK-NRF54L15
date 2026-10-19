# 传感器接口说明

本工程包含两类传感器的底层支持：

- **IMU：LIS2DW12TR**。使用 TWIM20 硬件 I2C 驱动，读取三轴加速度并输出串口日志。
- **PPG：GH3220**。通过供应商 SDK、OSAL 和 core framework 完成 SPI 通信、寄存器配置、算法处理与 BLE 协议传输。

## 管脚与总线

| 模块 | 信号 | GPIO | 软件配置 |
| --- | --- | --- | --- |
| LIS2DW12TR | ACC_SCL | P1.03 | TWIM20 SCL，100 kHz，外部 4.7 kΩ 上拉 |
| LIS2DW12TR | ACC_SDA | P1.06 | TWIM20 SDA，外部 4.7 kΩ 上拉 |
| LIS2DW12TR | ACC_INT | P1.04 | 输入，预留给数据就绪/事件中断 |
| LIS2DW12TR | ACC_INT2 | P1.05 | 可选中断输入；当前轮询模式未使用，软件不驱动该脚 |
| GH3220 | PPG_RST | P1.09 | 活性低复位输出；启动后保持高电平 |
| GH3220 | PPG_SPI_MISO | P1.10 | SPI21 MISO |
| GH3220 | PPG_SPI_CS | P1.11 | SDK 软件控制片选，低有效 |
| GH3220 | PPG_SPI_CLK | P1.12 | SPI21 SCK |
| GH3220 | PPG_SPI_MOSI | P1.13 | SPI21 MOSI |
| GH3220 | PPG_INT | P1.14 | 高有效中断输入；RTC polling 模式下保留 |

IMU 使用 TWIM20，通过 P1.03/P1.06 实现 100 kHz 硬件 I2C。UART20、SPI20 以及 I2C22
的 SPI/UART 可选功能均显式禁用。
自定义板定义不启用 DK 的板载 SPI NOR Flash、LED/按键和扩展排针，因此不会与 L-EVK
传感器管脚冲突。

P1.03 同时复用为 nRF54L15 的 NFC2 管脚；自定义板定义会禁用 NFCT，
并在第一次 I2C 访问前通过 `NFCT.PADCONFIG` 释放 NFC 管脚，否则 P1.03 不能由 TWIM20
正确驱动。

## LIS2DW12TR

硬件 I2C 与寄存器初始化在 `src/imu_lis2dw12.c` 中实现，默认参数如下：

| 项目 | 值 |
| --- | --- |
| I2C 地址 | 0x19 |
| I2C 控制器 | TWIM20，标准模式 100 kHz |
| 量程 | ±2 g |
| 输出数据率 | 25 Hz |
| 工作模式 | 高性能 |
| 应用读取方式 | 每 1 秒轮询；默认不输出样本，DBG 等级输出日志 |

LIS2DW12TR 的 7 位 I2C 地址由 **3 脚 SDO/SA0** 决定：拉低为 `0x18`，拉高为 `0x19`。本板图纸中 3 脚由 VDDIO 上拉，4 脚 `SDA/SDI/SDO` 接 `ACC_SDA`（P1.06），因此硬件 I2C 固定使用地址 `0x19`、SCL=P1.03、SDA=P1.06。2 脚 CS 已连接 VDDIO，也符合 I2C 模式要求。

启动时 TWIM20 读取 `WHO_AM_I`（寄存器 `0x0F`），正确值为 `0x44`；随后设置 25 Hz、高性能模式、±2 g 和 BDU。读取失败时优先检查 VDD（1.8 V）、VDDIO（1.8 V）、GND、SCL/SDA 焊接与两根 4.7 kΩ 上拉电阻。

正常连接时，115200 8N1 串口会输出初始化日志。将 `src/main.c` 中的
`L_EVK_LOG_LEVEL` 改为 `LOG_LEVEL_DBG` 后，每秒还会出现三轴样本：

~~~text
<inf> l_evk_imu: LIS2DW12TR ready: TWIM20 SCL=P1.03 SDA=P1.06, 100 kHz, ODR=25 Hz, +/-2 g
<inf> l_evk_imu: ACC[m/s2] X=+0.012 Y=-0.034 Z=+9.801
~~~

若日志显示 `hardware-I2C probe failed`，软件会在下一秒重新探测。请按上述供电、上拉、电气连接和地址逐项检查。ACC_INT（P1.04）已映射为输入；ACC_INT2（P1.05）是传感器的中断输出，当前轮询模式未使用，软件不会将它当作复位脚驱动。LIS2DW12TR 的 7 脚 `RES` 是保留脚，按数据手册应接 GND，并不是可由 MCU 驱动的复位输入。

### 示波器空载验证

TWIM20 的每次 `WHO_AM_I` 和三轴读取均在 P1.03/P1.06 产生标准 START、地址、ACK、
数据和 STOP 时序。若探测失败，日志会给出 Zephyr 硬件 I2C 返回码。

## GH3220

自定义 board 在 SPI21 下声明唯一的 `goodix,gh3220` 节点。驱动从 Devicetree 获取
SPI、CS、RESET 和 INT，不再使用应用内的临时 GPIO HAL。应用启动 core framework，
等待 SDK 初始化完成后启动 1 秒 RTC 调度；寄存器表参考
`l-evk-t2-gh3220/app_cardiff_a/src/gh3220_sdk`，算法库使用 `gh3220_sdk` 模块自带版本。

SPI21 使用 4 MHz、8 bit、MSB first、Mode 0（CPOL=0、CPHA=0）。P1.11 为低有效
GPIO，由 GH3220 SDK 在完整事务外层软件控制 CS；RESET 为 P1.09 低有效，INT 为 P1.14
高有效。当前 `CONFIG_GH3220_RTC_POLLING=y`，周期 tick 驱动健康处理，INT 管脚保留给
关闭 polling 后的中断模式。

当前配置沿用供应商示例的 GH3220 功能默认值，启用 HR、HRV、SpO2、ECG、NADT 等算法
依赖，并通过 GH3220 BLE GATT 服务传输协议数据。MCUmgr SMP OTA 和 GH3220 GATT 共用一次
Bluetooth 初始化：广播包携带 SMP UUID，扫描响应携带 GH3220 UUID 和缩短设备名
`L-EVK-54L`，完整 GAP 设备名仍为 `L-EVK-NRF54L15`。

预期启动日志包含：

~~~text
<inf> l_evk_blinky: GH3220 health framework started
<inf> gh3220_ble: BLE advertising started with SMP and GH3220 services
~~~

供应商静态算法库文件名标记为 GCC 12.2；本工程已经使用 NCS v3.4.1 的 GCC 14.3
完成编译、链接和实板运行。实板日志已验证 GH3220 FIFO 持续产生数据并生成多个算法帧；
算法数值准确性仍需用供应商工具、标准信号或已知测试数据校准验证。

## 构建与验证

~~~bash
cd <ncs-workspace>/ncs/apps/L-EVK-NRF54L15
./build.sh
./flash_daplink.sh
~~~

打开 UART30（115200 8N1），确认 LIS2DW12 初始化、GH3220 framework 启动并开始 BLE
广播。需要持续查看 LIS2DW12 三轴日志时启用 `LOG_LEVEL_DBG`。GH3220 通信可通过 FIFO
字节数和算法 frame counter 持续递增确认；算法结果准确性仍需在目标硬件上验证。
