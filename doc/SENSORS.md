# 传感器接口说明

本工程包含两类传感器的底层支持：

- **IMU：LIS2DW12TR**。使用 TWIM20 硬件 I2C 驱动，读取三轴加速度并输出串口日志。
- **PPG：GH3220**。本阶段只完成 SPI、复位和中断 GPIO 的初始化；不执行寄存器读写、不输出心率或血氧数据。

## 管脚与总线

| 模块 | 信号 | GPIO | 软件配置 |
| --- | --- | --- | --- |
| LIS2DW12TR | ACC_SCL | P1.03 | TWIM20 SCL，100 kHz，外部 4.7 kΩ 上拉 |
| LIS2DW12TR | ACC_SDA | P1.06 | TWIM20 SDA，外部 4.7 kΩ 上拉 |
| LIS2DW12TR | ACC_INT | P1.04 | 输入，预留给数据就绪/事件中断 |
| LIS2DW12TR | ACC_INT2 | P1.05 | 可选中断输入；当前轮询模式未使用，软件不驱动该脚 |
| GH3220 | PPG_RST | P1.09 | 活性低复位输出；启动后保持高电平 |
| GH3220 | PPG_SPI_MISO | P1.10 | SPI21 MISO |
| GH3220 | PPG_SPI_CS | P1.11 | SPI21 硬件片选，低有效 |
| GH3220 | PPG_SPI_CLK | P1.12 | SPI21 SCK |
| GH3220 | PPG_SPI_MOSI | P1.13 | SPI21 MOSI |
| GH3220 | PPG_INT | P1.14 | 输入；本阶段未注册中断回调 |

IMU 使用 TWIM20，通过 P1.03/P1.06 实现 100 kHz 硬件 I2C。UART20、SPI20 以及 I2C22
的 SPI/UART 可选功能均显式禁用。
自定义板定义不启用 DK 的板载 SPI NOR Flash、LED/按键和扩展排针，因此不会与 L-EVK
传感器管脚冲突。

P1.03 同时复用为 nRF54L15 的 NFC2 管脚，；自定义板定义会禁用 NFCT，
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
| 应用读取方式 | 轮询，每 1 秒输出一次日志 |

LIS2DW12TR 的 7 位 I2C 地址由 **3 脚 SDO/SA0** 决定：拉低为 `0x18`，拉高为 `0x19`。本板图纸中 3 脚由 VDDIO 上拉，4 脚 `SDA/SDI/SDO` 接 `ACC_SDA`（P1.06），因此硬件 I2C 固定使用地址 `0x19`、SCL=P1.03、SDA=P1.06。2 脚 CS 已连接 VDDIO，也符合 I2C 模式要求。

启动时 TWIM20 读取 `WHO_AM_I`（寄存器 `0x0F`），正确值为 `0x44`；随后设置 25 Hz、高性能模式、±2 g 和 BDU。读取失败时优先检查 VDD（1.8 V）、VDDIO（1.8 V）、GND、SCL/SDA 焊接与两根 4.7 kΩ 上拉电阻。

正常连接时，115200 8N1 串口每秒会出现类似日志：

~~~text
<inf> l_evk_imu: LIS2DW12TR ready: TWIM20 SCL=P1.03 SDA=P1.06, 100 kHz, ODR=25 Hz, +/-2 g
<inf> l_evk_imu: ACC[m/s2] X=+0.012 Y=-0.034 Z=+9.801
~~~

若日志显示 `hardware-I2C probe failed`，软件会在下一秒重新探测。请按上述供电、上拉、电气连接和地址逐项检查。ACC_INT（P1.04）已映射为输入；ACC_INT2（P1.05）是传感器的中断输出，当前轮询模式未使用，软件不会将它当作复位脚驱动。LIS2DW12TR 的 7 脚 `RES` 是保留脚，按数据手册应接 GND，并不是可由 MCU 驱动的复位输入。

### 示波器空载验证

TWIM20 的每次 `WHO_AM_I` 和三轴读取均在 P1.03/P1.06 产生标准 START、地址、ACK、
数据和 STOP 时序。若探测失败，日志会给出 Zephyr 硬件 I2C 返回码。

## GH3220

当前 src/ppg_gh3220_hal.c 只做以下操作：

1. 检查 SPI21 控制器已就绪；
2. 配置 P1.11 为活性低片选输出，并以高电平保持未选中；
3. 配置 P1.09 为活性低复位输出，并以高电平释放复位；
4. 配置 P1.14 为输入；
5. 记录 SPI21 管脚映射。

预期串口日志：

~~~text
<inf> l_evk_ppg_hal: GH3220 HAL ready: SPI21 SCK=P1.12 MOSI=P1.13 MISO=P1.10 CS=P1.11
<inf> l_evk_ppg_hal: GH3220 register driver is intentionally not enabled
~~~

本阶段不发送 SPI 事务，也不读取 GH3220 的芯片 ID、FIFO 或 PPG 数据，因此不会改变芯片工作寄存器。下一阶段实现 GH3220 驱动时，需要以芯片数据手册确认 PPG_INT 的有效电平、复位时序、SPI 模式/频率及寄存器初始化序列。

## 构建与验证

~~~bash
cd <ncs-workspace>/ncs/apps/L-EVK-NRF54L15
./build.sh
./flash_daplink.sh
~~~

打开 UART30（115200 8N1），确认 LIS2DW12 三轴日志持续输出，以及 GH3220 HAL 初始化日志出现。编译通过只证明软件配置和驱动接口正确；传感器是否可通信仍需要在已连接的目标硬件上按上述串口日志验证。
