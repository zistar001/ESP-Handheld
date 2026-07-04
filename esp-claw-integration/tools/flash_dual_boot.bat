@echo off
REM 双固件烧录：ESP-Handheld (factory) + ESP-Claw (ota_1)
REM
REM 分区布局：
REM   0x10000  factory  4MB  ← ESP-Handheld
REM   0x810000 ota_1    4MB  ← ESP-Claw
REM   0xC10000 emote   2.5MB ← ESP-Claw 表情
REM   0xE90000 system  768KB ← ESP-Claw 配置
REM   0xF50000 storage 704KB ← ESP-Claw 数据
REM
REM 使用方法：
REM   1. 修改下面 B 和 C 路径为你的实际构建目录
REM   2. 设备进入下载模式（按住 BOOT + 按 RESET）
REM   3. flash_dual_boot.bat COM8

if "%1"=="" (
    echo Usage: %~nx0 COM_PORT
    echo Example: %~nx0 COM8
    echo.
    echo 注意：请先修改脚本中的 B（ESP-Handheld 构建目录）
    echo 和 C（ESP-Claw 构建目录）路径！
    exit /b 1
)

set PORT=%1

REM ====== 请修改为你的实际路径 ======
set B=D:\code\claudecode\ESP_BSP_open\build
set C=D:\claw\application\edge_agent\build

echo ================================================
echo  双固件烧录：ESP-Handheld + ESP-Claw
echo  端口: %PORT%
echo ================================================
echo.
echo 确保设备已进入下载模式（按住 BOOT + 按 RESET）
echo.
pause

python -m esptool --chip esp32s3 -p %PORT% -b 460800 --before default_reset --after hard_reset write_flash ^
    --flash_mode dio --flash_size 16MB --flash_freq 80m ^
    0x0       "%B%\bootloader\bootloader.bin" ^
    0x8000    "%B%\partition_table\partition-table.bin" ^
    0xD000    "%B%\ota_data_initial.bin" ^
    0x10000   "%B%\esp_handheld.bin" ^
    0x810000  "%C%\edge_agent.bin" ^
    0xC10000  "%C%\emote_assets.bin" ^
    0xE90000  "%C%\system.bin" ^
    0xF50000  "%C%\storage.bin"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ===== 烧录成功！=====
    echo 默认启动到 ESP-Handheld
    echo 菜单 → ESP-Claw 切换到 AI 固件
    echo ESP-Claw 中按 START+B 返回
) else (
    echo.
    echo ===== 烧录失败 =====
)
pause
