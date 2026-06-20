@echo off
chcp 65001 >nul
cd /d "%~dp0"
echo Installing dependencies via uv...
uv venv
uv pip install sounddevice numpy pystray pillow
if %ERRORLEVEL% NEQ 0 (
    echo Install failed.
    pause
    exit /b 1
)
echo Done! Double-click Start_Voice_Receiver.vbs to launch.
pause
