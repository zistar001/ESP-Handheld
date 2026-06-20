# ESP32-S3 Handheld Game - 变更记录

## 2026-06-20 — 主页崩溃修复 + 键盘右键PTT + 按键映射重做

### 一、主页崩溃修复（严重）
- **`home_screen.c`**: 所有 `home_screen_update_*()` 函数增加 `lv_obj_is_valid(s_scr)` 守卫
  - 天气数据在后台加载完成时，若主页已被销毁（用户已进入菜单），不再写入悬空LVGL指针
  - 数据仍写入静态缓存变量，返回主页时可恢复显示

### 二、键盘模块右键PTT语音
- **PTT触发**: 长按右键>300ms → 启动WiFi语音流 + 发送 Ctrl+Shift+C
  - GPIO轮询检测按键状态，释放立即停止语音
  - 使用 `ble_hid_send_combo()` 带50ms时序间隔，避免BLE报文合并
- **键盘报告格式**: 改为标准8字节（6键槽），兼容输入法全局热键
- **按键映射重做**:
  - START → ESC（单按）
  - B → Backspace（退格）
  - A → Enter
  - 方向键 → 方向键
  - 长按右键 → 语音PTT + Ctrl+Shift+C
  - START+B → 退出应用
  - **移除**: 旧的长按A启动语音

### 三、PC语音接收
- `tools/pc_voice_receiver.py`: 提示文字更新
- `tools/pc_voice_receiver.exe`: 独立可执行文件（PyInstaller打包，无控制台窗口）

## 2026-06-11 — WiFi语音流 + 配网修复 + 音频优化

### 一、WiFi语音流（UDP → VB-Cable → Windows IME）
- **ESP32侧 `wifi_audio.c`**: UDP单播音频到PC，16kHz 16-bit mono
  - 动态IP发现：Config → PC IP 设置PC尾数，NVS持久化
  - PTT模式：按住A说话，松手停止
  - 麦克风增益自动拉到37dB（最大化SNR）
- **PC侧 `tools/pc_voice_receiver.py`**: UDP接收 → sounddevice → VB-Cable CABLE Input
  - 自动重采样16000→44100Hz
  - 输出到VB-Cable虚拟麦克风
- **音频质量修复**: channel_mask改为单麦(MASK(0))，参照XiaoZhi配置，消除噪声

### 二、配网修复
- BLE关掉后启动WiFi AP，避免2.4GHz干扰
- 修复 `wifi_do_reset()` 递归启动bug
- 优化配网界面显示AP名称和URL
- WiFi连接后不自动重启BLE（由用户重启设备）

### 三、UI改进
- Kbd App双按钮：Kbd ON/OFF + Voice状态显示
- Config → PC IP 尾数输入（UP/DOWN调节）
- 菜单新增Kbd/Mouse App入口

### 四、文件变更
- `main/modules/pc_remote/wifi_audio.c/h` — 重写，动态IP+PTT+NVS持久化
- `main/modules/audio/box_audio_codec.c` — channel_mask单麦修复
- `main/modules/wifi_manager/wifi_manager.c` — 修复配网启动
- `main/modules/wifi_manager/wifi_bridge.cpp` — WiFi连上后重建PC IP
- `main/app/app_manager.c` — 新增APP_ID_IP_INPUT
- `main/ui/screens/kbd_screen.c/h` — 双按钮+Voice指示器
- `main/ui/screens/ip_input.c/h` — **新建** PC IP尾数输入界面
- `main/ui/screens/settings_screen.c` — 新增PC IP设置项
- `main/ui/screens/airmouse_screen.c/h` — 重构为纯飞鼠App+灵敏度
- `tools/pc_voice_receiver.py` — **新建** PC端UDP音频接收器
- `tools/send_ip.py` — PC端BLE IP发送工具（备选）
- `docs/2026-06-09-pc-remote-architecture.md` — 更新架构文档

## 2026-06-10 — BLE HID 键鼠 + 双App重构

### 一、PC远控模块 (全新)
- **BLE HID键盘+鼠标** 通过 `ble_hid.c` 实现
  - 键盘报告 (Report ID 1)：`ble_hid_send_key(modifier, key)`
  - 鼠标报告 (Report ID 2)：`ble_hid_send_mouse(buttons, dx, dy)`
  - Just Works 配对（免密码），设备名 "ESP-Handheld"
- **空中飞鼠** `air_mouse.c` 使用IMU陀螺仪数据 → BLE鼠标
  - 零偏校准（100次采样平均）
  - 互补低通滤波
  - 死区1.5 dps滤除噪声
  - 灵敏度可调 (0.5/1.0/2.0)
  - 默认关闭，进入Mouse App后打开才启用
- **sdkconfig** 启用BT/Bluedroid/BLE 4.2
  - `CONFIG_BT_ENABLED=y`, `CONFIG_BT_BLUEDROID_ENABLED=y`
  - `CONFIG_BT_ALLOCATION_FROM_SPIRAM_FIRST=y`（BT内存走PSRAM节省内部DRAM）
  - `CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y`

### 二、双App拆分
- **⌨ Kbd App**：键盘HID模式
  - 开关Kbd ON/OFF
  - A=Enter，B=ESC，方向键=方向键
  - A长按=语音（预留）
- **🖱 Mouse App**：空中飞鼠模式
  - 开关Mouse ON/OFF
  - A=左键，B=右键，→=语音（预留）
  - 灵敏度三档 Low/Mid/High
- 菜单从4项扩展到6项：NES / Config / Kbd / Mouse / About / Timer
- 退出方式：START+B组合键（500ms窗口内）
- 退出时自动释放HID按键、关闭飞鼠

### 三、UI改进
- 所有中文字符改为英文（LVGL自带SimSun字体字库不全）
- Remote界面重写为独立Kbd/Mouse两个界面
- 按钮风格：灰色=OFF，橙色=ON

### 四、Bug修复
- **天气不显示**: gunzip解压缓冲区从8KB改为动态分配，修复API响应变大问题
- **菜单崩溃**: 修复app_manager中lvgl_lock死锁
- **飞鼠方向反了**: X轴取反
- **飞鼠灵敏度太高**: 系数从×4降到×2
- **鼠标乱飘**: 增加死区到1.5dps，默认关闭，进App才启用

### 五、文件变更
- `main/modules/imu/imu_driver.c/h` — 新增 `imu_is_ready()`
- `main/modules/pc_remote/ble_hid.c/h` — 新增 `ble_hid_mouse_click()`, `ble_hid_release_all()`
- `main/modules/pc_remote/air_mouse.c/h` — 新增 `air_mouse_set_enabled()`, `air_mouse_is_enabled()`
- `main/ui/screens/kbd_screen.c/h` — **新建** 键盘App界面
- `main/ui/screens/airmouse_screen.c/h` — 重写为鼠标App界面+灵敏度
- `main/ui/screens/menu_screen.c` — 扩展到6项
- `main/ui/screens/home_screen.c` — 字体改为Montserrat
- `main/app/app_manager.c/h` — 新增APP_ID_KEYBOARD
- `main/modules/weather/weather.c` — gunzip改为动态缓冲
- `sdkconfig` / `sdkconfig.defaults` — 启用BT/BLE配置
- `main/CMakeLists.txt` — 加入pc_remote和kbd_screen源文件

## 2026-06-08 — NES游戏系统重构

### 一、SD卡修复
- **问题:** SPI时钟4MHz过高，初始化超时
- **修复:** 降至400kHz（SD规范要求）；初始化顺序改为SD先→LCD后

### 二、视频渲染修复
- **问题:** PSRAM全帧缓冲DMA导致横条纹/花屏
- **修复:** 逐行渲染，栈变量`line_buf[240]`(内部SRAM)，每行独立DMA传输

### 三、游戏退出后卡死修复（关键）
- **问题1:** 退出回调只用一次 → 第二次退出时`s_exit_callback=NULL` → 画面卡死
  - **修复:** `nes_start()`每次调用重新注册`on_game_exit`
- **问题2:** 双重`app_manager_return()` → 状态乱跳
  - **修复:** `on_game_exit`检查状态，仅在`APP_STATE_RUNNING`时转换
- **问题3:** 加载中退出竞态 → `s_running`被游戏任务覆写 → SPI冲突
  - **修复:** 引入`s_abort`标志位，游戏任务在启动循环前检查

### 四、帧率精确控制
- vTaskDelay粗粒度(10ms) + spin-wait微秒补齐，保证稳定60fps

### 五、任务看门狗
- Core 1: 每帧`esp_task_wdt_reset()`喂狗
- Core 0: key_task优先级5→2

### 六、ST7789 panel级gap
- `esp_lcd_panel_set_gap(panel, 0, 20)`统一LVGL和NES坐标系
- LVGL的`offset_y`从20改为0

### 七、死代码清理
- 删除: `nes_port.cpp`, `nes_game_modified.c`, `CMakeLists.txt.bak`
- 统一实现: `nes_game.c` + `nes_wrapper.c`

---

## 2026-06-07 — 性能优化 (14fps → 目标60fps)

### 1. 内存分配优化 (components/nes_core/emucore.cpp)
- 模拟器核心结构体从 MALLOC_CAP_SPIRAM 改为 MALLOC_CAP_DMA：ppu_t, apu_t, mmc_t, nes_t, cpu, CPU RAM, SRAM, VRAM
- NesStateBlocks 保留在 SPIRAM (仅存档用)

### 2. 任务核心分配 (main/modules/nes/nes_game.c)
- video_task → Core 0 (LCD渲染)
- game_task → Core 1 (模拟器)
- 参考 esp32s3_nes_gamer 架构: 双核并行

### 3. 帧同步优化
- 移除同步阻塞，改为异步通知 (xTaskNotify)
- video_task 优先级从5降至1

### 4. 音频: 定时器驱动
- 独立 FreeRTOS 定时器(60Hz)，直接 i2s_channel_write()
- 绕过 codec 层降低延迟

### 5. BLE 共存优化
- 启动顺序: NES→BLE→WiFi (NES先分配DMA)

### 6. 系统稳定性
- 任务看门狗: 禁用IDLE检查 (模拟器占用CPU)

---

## 参考项目
- 性能架构: D:\code\claudecode\esp32s3_nes_gamer
- 驱动/WiFi: D:\code\claudecode\ESP_BSP\xiaozhi-esp32-main
- NES核心: D:\code\claudecode\Rachel (nofrendo参考)

## 已知问题
- WiFi init 可能因DMA不足失败 (BLE+NES占用DMA)
- 部分NES mapper不支持 (mapper 83等)
- NES颜色可能需要调`esp_lcd_panel_invert_color`标志
