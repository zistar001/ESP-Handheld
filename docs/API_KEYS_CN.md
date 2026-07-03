# API 密钥与外部服务

本固件可选地连接外部服务。以下是需要了解的信息。

## 必须注册的服务

**不需要。** 没有外部服务也能正常使用——天气功能会自动禁用。

## 推荐注册（完整功能）

### 和风天气 API

**用于：** 主屏天气显示

**注册步骤：**
1. 打开 [dev.qweather.com](https://dev.qweather.com)
2. 注册免费账号
3. 创建项目，获取 **API Key**（免费版每天 1000 次调用）

**配置方法：**
```bash
idf.py menuconfig
# 进入：ESP-Handheld Configuration → Weather API Key
# 输入你的 Key
```

或在运行时通过代码配置：
```c
weather_set_api_key("你的-api-key");
```

**工作原理：**
- API Key 在首次启动时存入 NVS（设备存储器）
- 配置后，天气每 30 分钟自动刷新一次
- 使用 HTTPS（TLS）加密传输

**API 详情：**
- 接口：`devapi.qweather.com`（端口 443）
- 路径：`/v7/weather/now`（实时天气）、`/v7/weather/7d`（7 天预报）
- 响应格式：gzip 压缩的 JSON

### NTP（时间同步）

**无需注册。** 固件使用以下 NTP 服务器：
- 主：`ntp.aliyun.com`
- 备：`pool.ntp.org`

WiFi 连接后自动同步时间。

## 注意事项

- 所有 API Key 存储在 NVS 中，不在源代码里
- WiFi 凭据存储在 NVS 的 `wifi` 命名空间
- 擦除 NVS（`nvs_flash_erase()`）会清除所有已配置的 Key
