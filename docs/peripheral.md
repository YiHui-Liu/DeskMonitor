# 外设说明
## SPI 总线
| 设备 | 片选信号 | SPI 时钟信号 | SPI 写数据 | SPI 读数据 |
| --- | --- | --- | --- | --- |
| 显示屏 | IO15 | IO14 | IO13 | IO12 |
| TF SD 数据卡 | IO5 | IO18 | IO23 | IO19 |
| 空余 SPI 总线 | IO21 | IO18 | IO23 | IO19 |

### 显示屏
* ESP-IDF `esp_lcd` + LVGL 驱动 ST7796S 面板
* 数据/命令信号：IO2，高电平表示数据，低电平表示命令
* 复位信号：EN，低电平复位，与 ESP32 EN 引脚共用
* 背光控制：IO27，高电平点亮

## I2C 总线
DeskMonitor 当前使用 ESP-IDF `i2c_master` 驱动访问板载传感器。I2C 总线在 `src/bsp/bsp_i2c.c` 中统一初始化，传感器驱动位于 `src/sensors/`，通过 `deskmon_i2c_bus()` 复用同一条总线。

| 信号 | ESP32 引脚 |
| --- | --- |
| SDA | IO32 |
| SCL | IO25 |

当前传感器读取是按需触发的：访问 `/api/diagnostics` 或打开 Web 控制台的传感器表格时，固件会读取一次传感器。暂时没有启用后台采样和历史存储。

### 传感器
| 传感器 | 物理量 | I2C 地址 | 寄存器 | 范围 | 灵敏度 | 分辨率 | 单位 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| TSL25911 | CH0 全光谱强度 | `0x29` | `0x14-0x15` | 0 - 88000 | 188 uLux | 1 | lux |
| TSL25911 | CH1 红外强度 | `0x29` | `0x16-0x17` | 0 - 88000 | 188 uLux | 1 | lux |
| ENS160 | TVOC | `0x53` | `0x22-0x23` | 0 - 65000 | 1 | 1 | ppb |
| ENS160 | eCO2 | `0x53` | `0x24-0x25` | 400 - 65000 | 1 | 1 | ppm |
| ENS160 | AQI | `0x53` | `0x21` | 1 - 5 | 1 | 1 | index |
| AHT20 | 温度 | `0x38` | | -40 - 85 | ±0.3 | 0.01 | °C |
| AHT20 | 湿度 | `0x38` | | 0 - 100 | ±2 | 0.024 | %RH |


### 换算公式
$$
\begin{aligned}
\mathrm{RH [\%]} = \frac{S_\mathrm{RH}}{2^{20}} \times 100 \\
\mathrm{T [°C]} = \frac{S_\mathrm{T}}{2^{20}} \times 200 - 50 \\
\mathrm{Counts\ Per\ Lux} = \frac{T \times Gain}{408.0} \\
L_1 = \frac{CH_0 - 1.7 * CH_1}{CPL} \\
L_2 = \frac{0.6 * CH_0 - CH_1}{CPL} \\
L = \max(L_1, L_2, 0) \\
\end{aligned}
$$

## IO
按键和 TF 相册功能仍只保留硬件定义，不在运行时初始化。

| IO | 功能 |  备注 |
| --- | --- | --- |
| IO35 | 按钮1 OUT | 预留，未初始化 |
| IO39 | 按钮2 OUT | 预留，未初始化 |
| IO22 | RGB 红色 | 低电平点亮 |
| IO16 | RGB 绿色 | |
| IO17 | RGB 蓝色 | |
| IO4 | Audio 使能 | 低电平有效 |
| IO26 | Audio DAC 输出 |  |
| IO3 | 串口接收 |  |
| IO1 | 串口发送 |  |
| IO34 | 电池电压 ADC 检测 |  |
