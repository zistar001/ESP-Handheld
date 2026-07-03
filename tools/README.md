# PC Tools

## WiFi Audio Receiver

The firmware can stream audio from the handheld's microphone to a PC over WiFi (UDP).

**Source:** `run_voice_receiver.pyw` — Python script (requires Python + tkinter)
**Alternative:** A prebuilt Windows executable is available separately (not in git — download from Releases).

Usage:
```bash
python run_voice_receiver.pyw
```

Protocol details: See `main/modules/pc_remote/wifi_audio.c`

## Utility Scripts

| Script | Purpose |
|--------|---------|
| `send_ip.py` | Send a target IP address to the device over serial |
| `gen_icons.py` | Generate icon assets for the firmware |
| `icons/` | PNG icon assets |
| `ui/` | UI design files (.pen) |
