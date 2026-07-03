# User Manual — ESP-Handheld Operation Guide

## 1. Button Layout

### Physical Buttons

```
              [START]                    [UP]
                                          ↑
    [LEFT] ←  [DOWN]  → [RIGHT]    [A]  [B]
                ↓
```

| Button | Location | GPIO | Function |
|--------|---------|------|----------|
| **UP** | Top-left | GPIO16 | Navigate up |
| **DOWN** | Bottom-left | GPIO15 | Navigate down |
| **LEFT** | Mid-left | GPIO17 | Navigate left |
| **RIGHT** | Mid-right | GPIO7 | Navigate right / confirm |
| **A** | Bottom-right | GPIO4 | Confirm / select |
| **B** | Bottom-right (left of A) | GPIO5 | Back / cancel |
| **START** | Top-left (above UP) | GPIO6 | Menu / start |

### Combo Keys

| Combo | Hold Time | Function |
|-------|-----------|----------|
| **START + B (anywhere)** | — | Exit current app / go back |
| **START + B (Retro-Go)** | 150ms | Return to ESP-Handheld |
| **START + B (XiaoZhi)** | 150ms | Return to ESP-Handheld |
| **START + A (in-game)** | — | Simulate SELECT button |
| **START (hold)** | 1s | Open game menu (Retro-Go) |
| **RIGHT (keyboard mode)** | — | Push-to-talk voice |

---

## 2. Navigation

### Home Screen (Launcher)

After boot, the home screen shows:
- **Status bar:** WiFi icon, time, Bluetooth status, battery
- **Weather card:** Forecast display (requires API key)
- **Indoor temp/humidity:** AHT20 sensor data

Press **START** → Enter app menu

### App Menu

9 app cards in 2-column grid layout:
```
┌─────────┬─────────┐
│  Games  │ XiaoZhi │
├─────────┼─────────┤
│ Keyboard│  Mouse  │
├─────────┼─────────┤
│ I Ching │ Liu Ren │
├─────────┼─────────┤
│ Timer   │Spectrum │
├─────────┼─────────┤
│ Settings│         │
└─────────┴─────────┘
```

- **UP/DOWN:** Navigate
- **A:** Select/launch app
- **B/START:** Return to home

---

## 3. App Usage

### Games (Retro-Go)

Launches Retro-Go game system (ota_0 partition). The device reboots to the game system.

**Supported platforms:** NES/Famicom, GB/GBC, SMS/GG, PCE/TurboGrafx-16, SNES/SFC

**Return:** Hold **START+B for 150ms** in Retro-Go.

### XiaoZhi AI

Launches XiaoZhi AI voice assistant (ota_1 partition).

**Return:** Hold **START+B for 150ms**.

### Keyboard (BLE HID)

Pairs as a Bluetooth keyboard. Enter the Keyboard app and pair with your device via Bluetooth settings (search for "ESP-Handheld").

**Key mapping:** UP/DOWN/LEFT/RIGHT = arrow keys, A = Enter, B = Esc, START = Tab

**Push-to-talk:** Hold **RIGHT** to speak through the mic (requires PC IP configuration).

### Mouse (BLE HID)

Uses the IMU gyroscope to control the mouse pointer. Tilt the device to move the cursor.

- **A:** Left click
- **B:** Right click
- **UP:** Increase sensitivity
- **DOWN:** Decrease sensitivity

### Fortune (I Ching)

Generates random hexagrams with interpretations.

### Xiao Liu Ren

Traditional Chinese divination based on three input numbers.

### Timer

- UP/DOWN: Set time
- A: Start/pause
- START+B: Exit

### Spectrum

Real-time FFT audio spectrum analyzer. Adjustable parameters: sensitivity, FFT size, smoothing, color.

### Settings

Navigate with UP/DOWN, select with A, back with B/START.

| Setting | Description |
|---------|-------------|
| Volume | Speaker volume (0-100) |
| Brightness | Display brightness (0-100) |
| Configure WiFi | Enter WiFi config mode |
| Weather Location | Select forecast location |
| Theme | Switch color theme (6 built-in) |
| Theme Colors | Customize 5 color slots |
| Key Mapping | Remap 7 physical buttons |
| Sleep Timeout | Auto-sleep delay (5s-2hr) |
| About | System info |

---

## 4. WiFi Configuration

### Via Settings
Settings → Configure WiFi → device creates hotspot → connect to "ESP-Handheld_XXXX" → open 192.168.4.1

### WiFi Audio (PC IP)
Settings → IP Input → enter last octet of PC's IP address

---

## 5. Power Management

**Light Sleep** activates after inactivity timeout (default 5 minutes):
1. Turns off backlight
2. Deinits Bluetooth
3. Stops WiFi
4. Enters Light Sleep (~5-10mA)

**Wake:** Press any button. All services resume automatically.

---

## 6. Firmware Switching

| System | Partition | Access |
|--------|-----------|--------|
| ESP-Handheld | factory | Default, or START+B from Retro-Go/XiaoZhi |
| Retro-Go | ota_0 | Menu → Games |
| XiaoZhi AI | ota_1 | Menu → XiaoZhi |
