# ESP32-S3 智能小手柄 — PC远控 + 语音输入 架构设计

> **更新日期:** 2026-06-10
> **状态:** Phase 1 完成，Phase 2 待开发

---

## 1. 概述

本项目将ESP32-S3智能小手柄作为PC的**无线输入外设**，实现三种输入方式：

- **键盘** — 物理按键通过BLE HID映射为PC键盘
- **鼠标** — IMU陀螺仪通过BLE HID映射为PC鼠标（空中飞鼠）
- **语音输入** — 麦克风音频通过WiFi UDP传输到PC，经虚拟声卡接入系统IME语音识别

### 应用场景

**Vibe Coding 权限确认：**
```
Agent弹窗 "Allow agent to run 'npm install'?" 
    → 按A键(Enter=允许) 或 B键(ESC=拒绝)
    → 1秒完成，无需鼠标
```

**Vibe Coding 语音输入：**
```
Agent问 "What should the variable name be?"
    → 按住B键说话 "用camelCase，叫userAuthToken"
    → 松手 → 语音→文字自动输入到IDE
```

---

## 2. 系统架构

```
┌─ ESP32-S3 Handheld ──────────────────────────────────────────────────────┐
│                                                                           │
│  ┌──────────┐    ┌──────────────┐    ┌─────────────────┐                  │
│  │ 物理按键   │───→│ Key Driver   │───→│ BLE HID Keyboard│ ──BLE──→ PC     │
│  │ 7键       │    │ (消抖+映射)   │    │ (Report ID 1)   │                 │
│  └──────────┘    └──────────────┘    └─────────────────┘                  │
│                                                                           │
│  ┌──────────┐    ┌──────────────┐    ┌─────────────────┐                  │
│  │ LSM6DS3   │───→│ Air Mouse    │───→│ BLE HID Mouse   │ ──BLE──→ PC     │
│  │ IMU       │    │ (滤波+校准)    │    │ (Report ID 2)   │                 │
│  └──────────┘    └──────────────┘    └─────────────────┘                  │
│                                                                           │
│  ┌──────────┐    ┌──────────────┐    ┌─────────────────┐                  │
│  │ ES7210   │───→│ I2S RX       │───→│ WiFi UDP Audio  │ ──WiFi──→ PC     │
│  │ 麦克风    │    │ (16kHz/16bit) │    │ (port 9210)     │                 │
│  └──────────┘    └──────────────┘    └─────────────────┘                  │
│                                                                           │
│  按键交互:                                                                 │
│    B键按住 → wifi_audio_start() + 显示 "🎤 录音中..."                      │
│    松B键   → wifi_audio_stop()                                            │
│    A键     → BLE HID Enter (允许)                                         │
│    B键(短按) → BLE HID ESC (拒绝/返回)                                    │
└───────────────────────────────────────────────────────────────────────────┘
                                     │
                  ┌──────────────────┴──────────────────────┐
                  │                                         │
          BLE HID Keyboard/Mouse                   WiFi UDP Audio
          (标准HID协议，无需驱动)                    (PCM 16kHz 16bit)
                  │                                         │
                  ▼                                         ▼
          ┌──────────────┐                        ┌──────────────────┐
          │ PC 自带蓝牙    │                        │ Python 接收服务    │
          │ 配对即可      │                        │ socket → sounddevice
          └──────────────┘                        └────────┬─────────┘
                                                           │ 写入
                                                  ┌────────▼─────────┐
                                                  │ CABLE Input       │
                                                  │ (VB-Audio虚拟播放)  │
                                                  └────────┬─────────┘
                                                           │ [Virtual Cable]
                                                  ┌────────▼─────────┐
                                                  │ CABLE Output      │
                                                  │ (系统麦克风设备)    │
                                                  └────────┬─────────┘
                                                           │ 选为输入
                                                  ┌────────▼─────────┐
                                                  │ Win+H            │
                                                  │ 系统IME语音输入    │
                                                  │ → 文字自动键入    │
                                                  └──────────────────┘
```

---

## 3. 技术路线验证

### 3.1 BLE HID 键鼠 ✅ 代码已实现

`ble_hid.c` 已包含完整的BLE HID实现：
- 键盘报告（Report ID 1）：`ble_hid_send_key(modifier, key)`
- 鼠标报告（Report ID 2）：`ble_hid_send_mouse(buttons, dx, dy)`
- 安全配对（Passkey 1234）
- 断线重连

**当前问题：** `CONFIG_BT_ENABLED` 在sdkconfig中被禁用（`# CONFIG_BT_ENABLED is not set`），需要启用。

### 3.2 空中飞鼠 ✅ 代码已实现

`air_mouse.c` 已包含完整算法：
- 零偏校准（100次采样平均）
- 互补低通滤波（0.7当前值 + 0.3历史值）
- 死区（0.5 dps）
- 灵敏度倍率（配置中的 `air_mouse_sens`）

**IMU当前状态：** 已确认工作 ✅
- 地址：0x6B（不是文档标注的0x6A）
- 芯片：LSM6DS3（WHO_AM_I=0x6A，不是预期0x6C）
- 加速度计 + 陀螺仪均正常

### 3.3 WiFi语音流 ✅ 代码骨架已实现

`wifi_audio.c` 已包含：
- `wifi_audio_start(ip, port)` — 启动UDP音频流
- `box_audio_read(buf, 256)` — 从ES7210读取PCM
- 16kHz 16bit 单声道，UDP发送

**需补充：**
- 按键触发（B键按住→流，松开→停）
- UI提示（🎤 录音中...）
- PC端接收服务

### 3.4 PC端语音接收服务

**技术方案确认：** VB-Cable (VB-Audio Virtual Cable) ✅

**工作原理：**
```
Python服务 → [CABLE Input] → [Virtual Cable] → [CABLE Output] → 系统麦克风
  写入音频      播放设备         虚拟通道         录音设备       任app选为输入
```

**核心代码模式（已验证可工作）：**
```python
import sounddevice as sd
import socket, numpy as np

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('0.0.0.0', 9210))

def callback(outdata, frames, time, status):
    data, addr = sock.recvfrom(frames * 2)
    outdata[:] = np.frombuffer(data, dtype=np.int16).reshape(-1, 1)

stream = sd.OutputStream(
    samplerate=16000, channels=1, dtype='int16',
    device='CABLE Input (VB-Audio Virtual Cable)',
    callback=callback
)
stream.start()
```

**依赖：**
- VB-Cable 免费版（一次安装）→ https://vb-audio.com/Cable/
- Python包：`sounddevice`, `numpy`

### 3.5 资源评估

#### GDMA通道
| 使用方 | 通道数 | 说明 |
|-------|--------|------|
| SPI2 (LCD+SD) | 1 TX | 共享总线 |
| I2S0 TX (ES8311) | 1 TX | 音频输出 |
| I2S0 RX (ES7210) | 1 RX | 音频输入 |
| **总计** | **3/5对** | ESP32-S3有5对GDMA，还有余量 |
| BLE | 0 | BLE控制器使用独立内部DMA，不占用GDMA |

#### 内部SRAM (约350KB可用)
| 使用方 | 估计占用 | 说明 |
|-------|---------|------|
| FreeRTOS + IRAM | ~100KB | 系统固定 |
| NES DMA结构体 | ~60KB | 跑NES时分配，PC远控时不分配 |
| WiFi | ~40KB | 需要时才启用 |
| BLE Bluedroid | ~90-100KB | 持续占用 |
| 音频I2S | ~10KB | 固定 |
| SPI/LCD DMA | ~8KB | 固定 |
| 任务栈 | ~20KB | 固定 |

**关键结论：** NES和PC远控是互斥场景，内存足够。

---

## 4. 按键映射方案

### ⌨ Kbd App

| 按键 | HID动作 | 功能 |
|------|--------|------|
| A(短按) | Enter | 确认/允许Agent |
| A(长按) | — | 🎤 语音输入（预留） |
| B | ESC | 返回/拒绝Agent |
| 方向键 | ↑/↓/←/→ | 方向键 |
| START+B | — | 退出Kbd模式 |
| 开关OFF | — | 按键控制UI（开关切换） |

### 🖱 Mouse App

| 按键 | HID动作 | 功能 |
|------|--------|------|
| A | 左键单击 | 确认/选择 |
| B | 右键单击 | 右键菜单 |
| → | — | 🎤 语音输入（预留） |
| 设备倾斜 | 鼠标移动 | IMU→光标 |
| START+B | — | 退出Mouse模式 |
| 开关OFF | — | 按键控制UI（开关/灵敏度） |

---

## 5. 实施路线图

### Phase 1 🔵 BLE HID 键鼠（已完成 ✅）
```
工作量: 小
依赖:  ESP-IDF v5.x, sdkconfig BT启用
```

**已完成：** 2026-06-10
- BLE HID配对（Just Works，免密码）
- ⌨ Kbd App（键盘HID，方向键+A=Enter+B=ESC）
- 🖱 Mouse App（空中飞鼠+左右键+三级灵敏度）
- START+B组合键退出
- 天气 gunzip 修复

**验证：**
- PC蓝牙搜索到 "ESP-Handheld" ✅
- 按键控制电脑光标 ✅
- 晃动设备，鼠标指针跟随移动 ✅
- 退出后释放HID按键 ✅

### Phase 2 🟢 WiFi语音流（后续）
```
工作量: 中 (~半天)
PC侧依赖: VB-Cable, Python 3.10+, sounddevice, numpy
ESP32侧: 代码已就绪，需补按键交互逻辑
```

1. **ESP32侧 — 按键交互**
   - 按键回调中检测B键按住/松开
   - 按住时：`wifi_audio_start(pc_ip, 9210)`
   - 松开时：`wifi_audio_stop()`
   - UI：LVGL显示 "🎤 录音中..."

2. **PC侧 — Python接收服务** (~80行)
   - UDP接收 → sounddevice → VB-Cable
   - 托盘图标运行
   - 自动开机启动

3. **配置**
   - PC的IP地址传递（通过BLE或mDNS）
   - UDP端口：9210

**验证方法：**
- 装好VB-Cable + PC服务
- Win+H开启语音输入
- 对手柄说话 → 文字出现在IDE中

### Phase 3 🟡 体验打磨（后续）
```
工作量: 小
```
- 一键流：拿起即用，无需手动切输入法
- 状态栏显示BLE/WiFi连接状态
- 低功耗：WiFi idle时自动省电

---

## 6. 环境搭建与编译烧录

### 6.1 ESP-IDF 环境要求

| 组件 | 版本 | 路径（参考） |
|------|------|-------------|
| ESP-IDF | v5.4.1+ | `D:\code\esp-idf-v5.4.1` 或 `D:\Espressif\frameworks\esp-idf-v5.5.4` |
| Python | 3.11+ | IDF自带环境 |
| CMake | 3.30+ | `C:\Espressif\tools\cmake\3.30.2\bin` |
| Ninja | 1.12+ | `C:\Espressif\tools\ninja\1.12.1` |
| 工具链 | xtensa-esp-elf 14.2.0+ | `C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20241119\bin` |

### 6.2 快速搭建（新电脑）

```powershell
# 1. 安装 ESP-IDF (v5.4.1 推荐)
# 下载: https://dl.espressif.com/dl/esp-idf/
# 或从现有安装复制:
#   D:\code\esp-idf-v5.4.1\  ← IDF本身
#   C:\Espressif\tools\       ← 工具链、cmake、ninja等

# 2. 验证环境
# 打开 Windows cmd.exe（不支持MSYS2/Git Bash）
set IDF_PATH=D:\code\esp-idf-v5.4.1
set IDF_TOOLS_PATH=C:\Espressif\tools
set PATH=C:\Espressif\tools\cmake\3.30.2\bin;%PATH%
set PATH=C:\Espressif\tools\ninja\1.12.1;%PATH%
set PATH=C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20241119\xtensa-esp-elf\bin;%PATH%
set PATH=C:\Espressif\tools\python_env\idf5.4_py3.12_env\Scripts;%PATH%
python %IDF_PATH%\tools\idf.py --version
```

### 6.3 编译

```bash
# 进入项目目录
cd D:\code\Claudecode\ESP_BSP

# 方法1：使用 PowerShell 脚本（推荐）
pwsh -ExecutionPolicy Bypass .\_build_debug.ps1

# 方法2：直接 cmd.exe
# 设置好环境变量后：
python %IDF_PATH%\tools\idf.py build

# 注意：
# - IDF_COMPONENT_MANAGER=1  （需要zlib等托管组件）
# - 已有 build/ 目录时，增量编译很快
# - 首次编译需联网下载组件（managed_components/zlib）
```

### 6.4 烧录

```bash
# 确认 COM 口（设备管理器查看）
# 常用: COM3 或 COM7

# 仅更新固件（推荐，不擦除配置）
python -m esptool --chip esp32s3 -p COM3 -b 921600 ^
  --before default_reset --after hard_reset write_flash ^
  --flash_mode dio --flash_size 16MB --flash_freq 80m ^
  0x10000 build\esp_handheld.bin

# 完整烧录（含bootloader/分区表）
python -m esptool --chip esp32s3 -p COM3 -b 921600 ^
  --before default_reset --after hard_reset write_flash ^
  --flash_mode dio --flash_size 16MB --flash_freq 80m ^
  0x0 build\bootloader\bootloader.bin ^
  0x8000 build\partition_table\partition-table.bin ^
  0xd000 build\ota_data_initial.bin ^
  0x10000 build\esp_handheld.bin
```

### 6.5 串口监视

```bash
# 方法1：idf.py monitor
python %IDF_PATH%\tools\idf.py -p COM3 monitor

# 方法2：直接Python（更稳定，避免idf.py退出问题）
python -c "
import serial, time
ser = serial.Serial('COM3', 115200, timeout=1)
ser.dtr = False; ser.rts = True; time.sleep(0.1); ser.rts = False
while True:
    line = ser.readline()
    if line: print(line.decode('utf-8', errors='replace').rstrip())
"
```

### 6.6 常用构建脚本

项目已有以下脚本（均在项目根目录）：

| 脚本 | 用途 | 说明 |
|------|------|------|
| `_build_debug.ps1` | 编译 | IDF v5.4.1, COMPONENT_MANAGER=1 |
| `_build_v5_4.ps1` | 编译 | IDF v5.4.1, COMPONENT_MANAGER=0 |
| `_bf.ps1` | 编译+烧录 | IDF v5.5.4（需对应环境） |
| `_flash_debug.ps1` | 仅烧录固件 | 快速更新app分区 |
| `_flash.ps1` | 完整烧录 | 含bootloader+分区表 |
| `_monitor_debug.ps1` | 串口监视 | 使用idf.py monitor |
| `_capture.py` | 串口捕获 | 过滤输出，30秒超时 |

### 6.7 跨电脑注意事项

```ini
; 不同电脑的环境差异：
; 1. IDF安装路径可能不同 -> 修改脚本中的 $env:IDF_PATH
; 2. 工具链路径可能不同  -> 修改 $env:IDF_TOOLS_PATH
; 3. COM口号可能不同    -> 修改 -p COM3 为实际端口
; 4. Python环境可能不同  -> 需找到正确的 python.exe路径
; 5. 组件管理器设置     -> 检查IDF_COMPONENT_MANAGER=1

; 快速定位工具链：
; where cmake
; where ninja
; where xtensa-esp32s3-elf-gcc
; where python
```

---

## 7. 相关文件

| 文件 | 功能 |
|------|------|
| `main/main.c` | 入口：初始化各模块 |
| `main/modules/pc_remote/ble_hid.c/h` | BLE HID 键盘+鼠标驱动 |
| `main/modules/pc_remote/air_mouse.c/h` | 空中飞鼠算法 |
| `main/modules/pc_remote/wifi_audio.c/h` | WiFi音频UDP流 |
| `main/modules/imu/imu_driver.c/h` | LSM6DS3 IMU驱动 |
| `main/modules/audio/box_audio_codec.c/h` | 音频编解码器 |
| `main/ui/screens/airmouse_screen.c/h` | 空中飞鼠UI界面 |
| `main/app/app_manager.c/h` | 应用管理器 |
| `docs/project-reference.md` | 项目参考手册 |
| `sdkconfig` | ESP-IDF配置（需启用BT） |
| `sdkconfig.defaults` | 默认配置模板 |

---

## 8. 参考资源

- VB-Cable: https://vb-audio.com/Cable/
- Python sounddevice: https://python-sounddevice.readthedocs.io/
- ESP-IDF BLE HID示例: `examples/bluetooth/esp_hid_device/`
- ESP32-S3手册: https://www.espressif.com/en/products/socs/esp32-s3
