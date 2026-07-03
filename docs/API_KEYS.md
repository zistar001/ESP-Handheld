# API Keys & External Services

This project optionally connects to external services. Below is what you need.

## MUST Register

None. The firmware works without any external services — weather will simply be disabled.

## SHOULD (for full features)

### HeFeng Weather API

**Required for:** Weather dashboard on home screen

**Registration:**
1. Go to [devapi.qweather.com](https://devapi.qweather.com)
2. Sign up for a free account
3. Create a project and get your **API Key** (free tier: 1000 calls/day)

**Configuration:**
```bash
idf.py menuconfig
# Navigate to: ESP-Handheld Configuration → Weather API Key
# Enter your key
```

Or configure at runtime:
```c
weather_set_api_key("your-api-key-here");
```

**How it works:**
- API key is stored in NVS on first boot
- After configuration, the weather refreshes every 30 minutes
- Uses HTTPS (TLS) with ESP证书绑定

**API Details:**
- Endpoint: `devapi.qweather.com` (port 443)
- Endpoints: `/v7/weather/now` (current), `/v7/weather/7d` (7-day forecast)
- Response format: gzip-compressed JSON

### NTP (Time Sync)

**No registration needed.** The firmware uses:
- Primary: `ntp.aliyun.com`
- Fallback: `pool.ntp.org`

Time sync happens automatically after WiFi connects.

## Notes

- All API keys are stored in NVS, not in source code
- WiFi credentials are stored in NVS under the `wifi` namespace
- Deleting NVS (`nvs_flash_erase()`) will clear all configured keys
