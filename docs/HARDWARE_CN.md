# 硬件适配指南

## 参考 PCB（Rev 2 版本）

本固件为自制 ESP32-S3 掌机 PCB（Rev 2）开发。以下是完整的引脚定义。

### 引脚定义表

| 功能 | 接口 | 引脚 | 说明 |
|------|-----------|------|------|
| **LCD 背光** | — | GPIO43 | PWM 调光 |
| **LCD DC** | SPI2_HOST | GPIO44 | 数据/命令选择 |
| **LCD CS** | SPI2_HOST | GPIO42 | 片选 |
| **LCD SCK** | SPI2_HOST | GPIO21 | SPI 时钟 |
| **LCD MOSI** | SPI2_HOST | GPIO14 | SPI 数据 |
| **LCD RST** | — | GPIO13 | 复位 |
| **SD CS** | SPI3_HOST | GPIO38 | 片选 |
| **SD CLK** | SPI3_HOST | GPIO40 | SPI 时钟 |
| **SD MOSI** | SPI3_HOST | GPIO39 | SPI 数据 |
| **SD MISO** | SPI3_HOST | GPIO41 | SPI 数据 |
| **麦克风 BCLK** | I2S_NUM_0 | GPIO11 | MSM261 位时钟 |
| **麦克风 WS** | I2S_NUM_0 | GPIO12 | MSM261 字选择 |
| **麦克风 DATA** | I2S_NUM_0 | GPIO10 | MSM261 数据 |
| **功放 BCLK** | I2S_NUM_1 | GPIO46 | MAX98357 位时钟 |
| **功放 LRC** | I2S_NUM_1 | GPIO9 | MAX98357 左右声道时钟 |
| **功放 DIN** | I2S_NUM_1 | GPIO3 | MAX98357 数据 |
| **I2C SDA** | I2C | GPIO1 | IMU + AHT20 |
| **I2C SCL** | I2C | GPIO2 | IMU + AHT20 |
| **按键 UP** | — | GPIO16 | 上拉，低电平触发 |
| **按键 DOWN** | — | GPIO15 | 上拉，低电平触发 |
| **按键 LEFT** | — | GPIO17 | 上拉，低电平触发 |
| **按键 RIGHT** | — | GPIO7 | 上拉，低电平触发 |
| **按键 A** | — | GPIO4 | 上拉，低电平触发 |
| **按键 B** | — | GPIO5 | 上拉，低电平触发 |
| **按键 START** | — | GPIO6 | 上拉，低电平触发 |
| **BOOT** | — | GPIO0 | 下载模式 |
| **LED** | — | GPIO18 | 指示灯 |

### I2C 地址表

| 设备 | 地址 | 说明 |
|------|------|------|
| AHT20 | 0x38 | 温湿度传感器 |
| LSM6DS3TR-C | 0x6A | 六轴陀螺仪+加速度计 |

### 适配你自己的 PCB

1. **编辑 `main/bsp/bsp_board.h`** — 修改 `BSP_*` 引脚定义以匹配你的 PCB
2. **如果使用不同的屏幕**，还需修改：
   - `main/bsp/st7789_driver.c` — 显示驱动初始化序列
   - `main/CMakeLists.txt` — 如果使用其他显示驱动
3. **如果音频配置不同：**
   - `main/modules/audio/box_audio_codec.c` — I2S 配置
   - 如果使用外部音频编解码芯片（ES8311 等），恢复旧的驱动代码
4. **按键映射：**
   - 7 个物理按键使用简单的 GPIO 矩阵，在 `bsp_board.h` 中修改引脚定义
   - 运行时按键映射可在 设置 → 按键映射 中调整

### 屏幕说明

- ST7789 内存为 240×320，玻璃显示区域为 240×280（行 20-299）
- RGB565 颜色需要字节交换（ST7789 要求 MSB 在前）
- LVGL 配置在 `components/lv_conf.h`
- 显示驱动在 `main/bsp/st7789_driver.c`

### 音频说明

- 两个独立 I2S 控制器：I2S_NUM_0（麦克风 RX），I2S_NUM_1（功放 TX）
- Rev 2 PCB 无外部音频编解码芯片
- 麦克风增益：软件乘法器（0-37dB → 1.0x-70x）
- MAX98357 需要 BCLK 启动后约 12ms 的稳定时间
