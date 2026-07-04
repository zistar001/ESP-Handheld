# 🦞 ESP-Claw 使用说明

> 针对 Zistar ESP-Handheld 硬件（240×280 屏 + 喇叭 + 麦克风）

---

## 一、开机与连接

### 首次启动

烧录完成后设备会自动重启。启动顺序：

1. **屏幕显示** → ESP-Claw 启动动画
2. **WiFi 热点** → 设备创建一个名为 `esp-claw-XXXX`（XX 为 MAC 地址）的开放热点
3. **等待配置** → 需连接热点并配置 WiFi 和 LLM

### 连接配置

**方法 1：连接热点配置（首次使用）**

1. 用手机/电脑搜索 WiFi 热点 `esp-claw-XXXX`（无密码）
2. 连接后浏览器访问 `http://192.168.4.1`
3. 进入 Web 配置页面

**方法 2：局域网访问（已配网）**

设备连接上 WiFi 后，在浏览器访问：
```
http://esp-claw.local/
```
或查看路由器分配的 IP 地址（串口日志中也会打印）。

### 串口 Console

把设备通过 USB 连到电脑，使用串口终端（115200 baud）连接：
```
idf.py -p COM8 monitor
```
进入交互提示符（默认 `app>`）后可输入命令。

---

## 二、Web 配置页面

访问 `http://esp-claw.local/` 进入 Web 控制台，主要功能：

| 页面 | 功能 |
|------|------|
| 系统状态 | 查看网络信息、IP、WiFi 模式、运行状态 |
| 在线聊天 | 直接在网页中与 ESP-Claw 对话（不需要配置 IM） |
| 系统设置 | WiFi、SoftAP、LLM API Key、IM 绑定等 |
| 记忆管理 | 查看/编辑「灵魂」「身份」「用户信息」等记忆文件 |
| Capabilities | 启用/禁用 ESP-Claw 的各项能力模块 |
| 文件管理 | 查看和读写文件系统中的文件 |

### 基础设置

**WiFi 设置**：SSID + 密码（仅支持 2.4GHz）
**SoftAP 设置**：可修改热点名、密码。建议启用「连接 WiFi 后自动关闭 SoftAP」提高安全性
**时区设置**：格式为 POSIX TZ string，中国大陆为 `CST-8`

### LLM 设置（关键！）

需要配置 LLM 后才能使用对话功能。

**内置供应商（只需填写 API Key + 模型名）：**

| 供应商 | 推荐模型 | 获取 API Key |
|--------|----------|-------------|
| 阿里云百炼 | `qwen-plus` / `qwen3-plus` | [bailian.console.aliyun.com](https://bailian.console.aliyun.com/) 🆓 新用户赠 100 万 Token |
| OpenAI | `gpt-5.4` / `gpt-4o` | [platform.openai.com](https://platform.openai.com/) |
| Anthropic | `claude-sonnet-4-20250514` | [console.anthropic.com](https://console.anthropic.com/) |
| DeepSeek | `deepseek-chat` | [platform.deepseek.com](https://platform.deepseek.com/) |
| 硅基流动 | `Qwen/Qwen2.5-7B-Instruct` 等 | [siliconflow.cn](https://siliconflow.cn/) 🆓 有免费额度 |

**自定义供应商**：支持 OpenAI 兼容 API 和 Anthropic 兼容 API，需提供 Base URL。
- Base URL 示例：`https://api.openai.com/v1`（不含 `/chat/completions`）
- 模型选能力强的：`gpt-5.4`、`qwen3-plus`、`claude-4-sonnet`、`deepseek-v4-pro` 级别或更高

> ⚠️ 配置后需要点击「保存」并重启设备才生效。

---

## 三、使用方式

### 方式 1：Web 在线聊天

配置好 LLM 后，在 Web 页面的「在线聊天」中直接输入文字与 ESP-Claw 对话。

### 方式 2：串口 Console

通过 USB 串口连接，在 `app>` 提示符下：
```
ask 你好，介绍一下你的能力
ask_once 这是一次性问题，不会记入历史
```

### 方式 3：IM 聊天（推荐日常使用）

配置 IM 平台后，通过微信/Telegram/QQ/飞书 与 ESP-Claw 聊天。

**微信配置**：在 Web 配置页 → IM 设置 → 微信 → 生成二维码 → 微信扫码绑定
**Telegram**：在 [@BotFather](https://t.me/botfather) 创建 Bot → 获取 Token → 填入配置页

---

## 四、核心功能

### 💬 聊天答疑
像与人聊天一样提问：
```
- 你好，介绍一下你当前有哪些能力？
- 帮我计算：12% 年化利率下，10000 元 3 年后会变成多少钱？
```

### 🔍 联网搜索
需配置搜索 API（Tavily 或 Brave Search）：
```
- 最近有什么 AI 新闻？
- 帮我查一下今天的天气
- 访问 https://github.com/espressif/esp-claw 看看最新进展
```

### 📦 云端 Skills（技能安装）
```
- 你能获取一些云端技能吗？
- 安装天气技能
- 安装 GPIO 控制技能
```
安装后功能自动扩展。

### 🧠 记忆系统
```
- 我叫张三，记住我
- 我喜欢吃辣的
- 我在 GPIO4 上接了一个 WS2812 灯带
```
后续对话会自动调用这些记忆。

### ⏰ 定时任务
```
- 每隔五分钟发一条消息给我
- 每天早上 8 点总结当天的信息
- 明天上午十点提醒我开会
```

### ⚡ 自动化规则
```
- 添加自动化规则：下次我说"观影模式"时，立刻把灯光调成蓝色
```
这类规则不经过大模型，响应快、功耗低。

### 💻 Chat Coding（聊天造物）

ESP-Claw 最强大的功能 —— 用自然语言编程控制硬件：

```
- 点亮 GPIO18 上的 LED
- 让显示屏显示一个爱心图案
- 做一个能够显示天气和时间的桌面屏幕
- 当 GPIO4 按键按下时，切换 LED 状态
```

**Chat Coding 原则：**
1. **先描述意图**，不纠结语法
2. **先跑通**，再优化
3. **小步快跑**，复杂需求拆分成多轮对话
4. **提供反馈**，运行结果不符合预期就告诉它

### 📐 会话管理
```
/session      # 查看当前会话
/session new  # 创建新会话
```

---

## 五、屏幕与音频功能

### 屏幕状态指示
- 屏幕会显示 ESP-Claw 的表情/状态（连接中、思考中、响应等）
- 连接 WiFi 后状态会更新

### 音频输出（喇叭）
ESP-Claw 支持 TTS 语音回复（需 LLM 和音频模块支持）。
说话时会通过 MAX98357 功放播放。

### 音频输入（麦克风）
MSM261 数字麦克风已配置，可在 Chat Coding 中添加语音控制功能：
```
- 当我说"你好小龙虾"时，回应"你好主人"
- 检测到拍手声时打开 LED
```

---

## 六、常见问题

### 连不上 WiFi
- ESP32-S3 仅支持 **2.4GHz** 频段（不支持 5GHz）
- 如果连接失败，检查 SSID 和密码是否正确
- 可连接 SoftAP 重新配置

### 屏幕不显示/显示异常
- 检查 /board 配置是否正确
- 首次启动可能需要等几秒
- 如果画面偏移或花屏，需要调整 `setup_device.c` 中的偏移参数

### LLM 无响应
- 检查 API Key 是否正确
- 检查模型名称是否正确
- 检查账户余额/免费额度是否充足
- Web 配置页 → 系统状态 查看 LLM 是否初始化成功

### 音频不出声
- 检查 MAX98357 连接（BCLK=46, LRC=9, DIN=3）
- 音量设置可能在默认 0，通过对话调整：`把音量调到 80%`

### Token 消耗太快
- 使用国产模型（通义千问、DeepSeek）费用更低
- 配置搜索 API（Tavily/Brave）减少不必要的联网消耗

---

## 七、恢复 ESP-Handheld

如果想回到原来的 ESP-Handheld 固件，按照原来的烧录步骤重新烧录即可：

```bash
cd D:\code\claudecode\ESP_BSP_open
python -m esptool --chip esp32s3 -p COM8 -b 921600 write_flash \
  --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0xd000 build/ota_data_initial.bin \
  0x10000 build/esp_handheld.bin
```

---

## 八、相关链接

- [ESP-Claw 官方文档](https://esp-claw.com/)
- [ESP-Claw GitHub](https://github.com/espressif/esp-claw)
- [ESP-Claw 在线烧录](https://esp-claw.com/flash/)
- [阿里云百炼（免费 LLM）](https://bailian.console.aliyun.com/)
- [Tavily 搜索 API（免费额度）](https://app.tavily.com/)
