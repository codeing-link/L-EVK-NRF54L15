# Bluetooth LE OTA 升级指南

## 1. 功能说明

工程采用 NCS/Zephyr 官方 MCUboot + MCUmgr SMP 方案实现蓝牙低功耗 OTA：

```text
手机（nRF Device Manager）
        │ BLE SMP GATT 服务
        ▼
L-EVK 应用（MCUmgr） ──写入──> image-1 备用槽
        │ 重启并标记为测试镜像
        ▼
MCUboot ──验证签名──> 启动新镜像（未确认时可自动回滚）
```

启动后设备广播 `L-EVK-NRF54L15` 和官方 SMP 服务 UUID。当前 NCS 的 nRF Device
Manager 手机流程优先上传本工程 sysbuild 生成的 `build/dfu_application.zip`。使用
命令行 MCUmgr 或明确支持单镜像的客户端时，可上传其中对应的
`zephyr.signed.bin`；不要上传 `zephyr.bin`、`zephyr.hex` 或
`zephyr.signed.hex`。

`dts/l_evk_ota_partitions.overlay` 同时被应用与 MCUboot 显式引用，使用与 NCS
`dfu/smp_svr` nRF54L15 DK 样例相同的内部 RRAM 分区：

| 区域 | 起始地址 | 大小 | 用途 |
| --- | ---: | ---: | --- |
| MCUboot | `0x000000` | 68 KiB | 签名验证、启动与回滚 |
| image-0 | `0x011000` | 696 KiB | 当前运行的应用镜像 |
| image-1 | `0x0BF000` | 696 KiB | 通过 BLE 接收的新镜像 |
| storage | `0x16D000` | 64 KiB | 预留存储区 |

## 2. 准备条件

1. 使用本工程构建并完成一次有线烧写，确保 MCUboot 已安装。
2. 手机安装 Nordic **nRF Device Manager**（Android 或 iOS）。该应用支持 MCUmgr
   SMP 蓝牙固件管理。
3. 手机开启蓝牙，并靠近设备；升级期间保持供电稳定。
4. 打开串口终端（115200 8N1）可观察蓝牙连接、断开及应用启动日志，但 OTA 本身不
   使用串口。

## 3. 首次安装 OTA 固件

首次烧写必须写入完整镜像，不能只烧应用 `zephyr.hex`：

```bash
cd ncs/apps/L-EVK-NRF54L15
PRISTINE=always ./build.sh && ./flash_daplink.sh
```

成功启动后，检查串口中的 `BLE advertising started with SMP and GH3220 services` 日志，确认手机能扫描到
`L-EVK-NRF54L15`。

## 4. 生成一个可升级版本

每次发布新版本都应递增 MCUboot 镜像版本。以下示例将版本设为 `1.0.1+0`，并全新更新
默认 `build/` 目录：

```bash
PRISTINE=always ./build.sh -- \
  -DCONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION=\"1.0.1+0\"
```

推荐给 nRF Device Manager 上传的文件是：

```text
build/dfu_application.zip
```

原始的单应用签名镜像是：

```text
build/L-EVK-NRF54L15/zephyr/zephyr.signed.bin
```

发布前建议保留该文件、源码提交号、版本号和生成日期。不要更改 `app.overlay` 中的
槽位布局后再尝试给已部署设备做 OTA；分区布局变化应作为一次完整有线升级处理。

普通有线固件和 OTA 包均由同一次默认 `build/` 产生：有线烧写使用
`zephyr.signed.hex`，nRF Device Manager 使用 `dfu_application.zip`，命令行或单镜像
客户端使用 `zephyr.signed.bin`。除非为了归档发布版本，不需要创建另一个构建目录。

## 5. 用手机执行升级

不同版本的 nRF Device Manager 菜单文字可能略有差异，但流程相同：

1. 打开应用并扫描附近设备，选择 `L-EVK-NRF54L15`。
2. 打开设备的 **Firmware update / DFU** 功能，优先选择本机中的
   `dfu_application.zip`。
3. 开始上传，保持手机靠近设备并避免关闭应用。上传时设备会保持连接；完成后工具会
   请求设备重启。
4. 设备重启后 MCUboot 验证并试运行 image-1。重新连接串口，确认新版本的日志和 LED
   功能正常。
5. 再次连接 Device Manager，检查 image 列表：新镜像应为 active 且 confirmed。若工具
   提供 **Confirm** 操作，请在功能验证完成后执行确认。

本工程使用 MCUboot 的测试升级模式：如果新镜像未确认且随后再次复位，MCUboot 可以
回到已确认的旧镜像。这是升级异常时的保护机制，不能替代稳定供电和完整测试。

## 6. 命令行方式（可选）

如已安装支持 Bluetooth MCUmgr 的客户端，也可连接设备并执行与手机等价的
`image list`、`image upload`、`image test`、`reset` 和 `image confirm` 操作。命令的
具体形式随客户端版本不同。命令行单镜像上传使用 `zephyr.signed.bin`，并按以下顺序
执行：

```text
读取镜像列表 → 上传 image-1 → 设为 test/pending → 重启 → 验证 → confirm
```

若上传失败，断开后设备会重新开始广播；无需重新有线烧写。若 image-1 已存在残留，可
重新上传同一完整签名文件。

## 7. 安全注意事项

当前配置优先保证现场升级易用性：SMP GATT 服务允许未配对客户端读写。因此，任何处于
蓝牙范围内的设备都可能发起升级请求；虽然 MCUboot 仍会拒绝不匹配签名的镜像，但**不应
将当前配置直接用于生产环境**。

生产部署至少应完成以下工作：

1. 使用项目自己的永久 MCUboot 签名密钥；绝不可使用测试密钥发布产品。
2. 启用 BLE 配对/绑定及加密，并把 `CONFIG_MCUMGR_TRANSPORT_BT_PERM_RW` 改为
   `CONFIG_MCUMGR_TRANSPORT_BT_PERM_RW_AUTHEN=y` 或按照安全方案使用加密权限。
3. 实现适当的配对交互（例如 passkey）并限制可升级客户端。
4. 建立版本策略和回归测试；必要时启用 MCUboot 防降级保护。
5. 对固件包进行发布前签名、版本记录和可追溯性管理。

更改签名密钥或 MCUmgr 蓝牙访问权限会影响已部署设备的兼容性，应先在测试设备上验证。

## 8. 故障排查

| 现象 | 检查方式 |
| --- | --- |
| 手机扫描不到设备 | 观察串口是否有 `BLE advertising started with SMP and GH3220 services`；确认未被另一台手机保持连接；复位设备后重试。 |
| 上传时断开 | 缩短手机距离、保证供电、关闭其他蓝牙连接；设备断开后会重新广播。 |
| 工具拒绝镜像 | nRF Device Manager 优先选择 `dfu_application.zip`；命令行客户端选择 `zephyr.signed.bin`；检查镜像未超过 696 KiB 槽位并采用相同 MCUboot 签名链。 |
| 重启后仍是旧版本 | 新镜像可能未被标记 test/confirmed，或启动验证失败；使用 image list 检查状态，保留串口 MCUboot 日志。 |
| 首次 OTA 无法启动 | 先用 `./flash_daplink.sh` 写入 MCUboot 与 `zephyr.signed.hex`。 |
