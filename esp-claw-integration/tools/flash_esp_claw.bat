@echo off
REM ESP-Claw 完整烧录（独立模式 — 不保留 ESP-Handheld）
REM 使用方法：flash_esp_claw.bat COM8

if "%1"=="" (
    echo Usage: %~nx0 COM_PORT
    echo Example: %~nx0 COM8
    exit /b 1
)

set PORT=%1
set BUILD_DIR=D:\claw\application\edge_agent\build

echo ========================================
echo  ESP-Claw 完整烧录到 %PORT%
echo ========================================
echo.
echo 警告：这将覆盖整个 16MB flash，仅保留 ESP-Claw！
echo 确保设备已进入下载模式（按住 BOOT + 按 RESET）
echo.
pause

python -m esptool --chip esp32s3 -p %PORT% -b 460800 --before default_reset --after hard_reset write_flash ^
    --flash_mode dio --flash_size 16MB --flash_freq 80m ^
    0x0      %BUILD_DIR%\bootloader\bootloader.bin ^
    0x8000   %BUILD_DIR%\partition_table\partition-table.bin ^
    0xD000   %BUILD_DIR%\ota_data_initial.bin ^
    0x20000  %BUILD_DIR%\edge_agent.bin ^
    0x820000 %BUILD_DIR%\emote_assets.bin ^
    0xB20000 %BUILD_DIR%\system.bin ^
    0xC20000 %BUILD_DIR%\storage.bin

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ===== 烧录成功！=====
    echo 重启后 ESP-Claw 将启动。
    echo 连接热点: esp-claw-xxxx
    echo 浏览器访问: http://192.168.4.1 配置 WiFi 和 LLM API Key
) else (
    echo.
    echo ===== 烧录失败 =====
)
pause
