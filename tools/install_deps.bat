@echo off
chcp 65001 >nul
REM ESP32 Voice Receiver — 一键安装依赖
echo Installing dependencies...
python -m pip install sounddevice numpy pystray pillow
if %ERRORLEVEL% NEQ 0 (
    echo Install failed. Try: python -m pip install sounddevice numpy pystray pillow
    pause
    exit /b 1
)
echo Done! Double-click Start_Voice_Receiver.vbs to launch (no console).
pause
