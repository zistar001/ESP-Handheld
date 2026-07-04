@echo off
REM 从 ESP-Claw 切换回 ESP-Handheld（重置 OTAdata 到 factory）
REM 当 ESP-Claw 运行时，按住 BOOT + 按 RESET 进入下载模式
REM 然后运行此脚本：switch_to_esp_handheld.bat COM8

echo ================================
echo  切换回 ESP-Handheld
echo ================================
echo.
echo 1. 按住 BOOT 键 + 按 RESET 进入下载模式
echo 2. 运行: switch_to_esp_handheld.bat COM8
echo.

set PORT=COM8
if not "%1"=="" set PORT=%1

python -m esptool --chip esp32s3 -p %PORT% -b 460800 write_flash 0xD000 ota_data_initial.bin

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ===== 成功！设备将启动到 ESP-Handheld =====
    echo 拔掉 USB 重新上电即可
) else (
    echo.
    echo ===== 失败，请检查端口和连接 =====
)
pause
