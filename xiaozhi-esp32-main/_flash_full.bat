@echo off
set MSYSTEM=
set MINGW_PREFIX=
set MINGW_CHOST=
set MSYS2_PATH=
set MSYSCON=
set IDF_PATH=D:\Espressif\frameworks\esp-idf-v5.5.4
set IDF_TOOLS_PATH=D:\Espressif\tools
set IDF_PYTHON_ENV_PATH=D:\Espressif\tools\python_env\idf5.5_py3.11_env
set IDF_COMPONENT_MANAGER=1
set PYTHON=C:\Users\Administrator\.espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe
set PATH=D:\Espressif\tools\cmake\3.30.2\bin;D:\Espressif\tools\ninja\1.12.1;D:\Espressif\tools\idf-git\2.44.0\cmd;D:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin;D:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\libexec\gcc\xtensa-esp-elf\14.2.0;C:\Users\Administrator\.espressif\python_env\idf5.5_py3.11_env\Scripts;C:\windows\system32;C:\windows;C:\windows\System32\Wbem;%PATH%
cd /d D:\code\claudecode\ESP_BSP\xiaozhi-esp32-main

echo.
echo ============================================
echo  FULL ERASE + FLASH (16MB)
echo ============================================
echo.
echo Step 1: Erasing entire 16MB flash...
%PYTHON% -m esptool --chip esp32s3 -p COM7 erase_flash
if %errorlevel% neq 0 (
    echo ERASE FAILED!
    pause
    exit /b 1
)

echo.
echo Step 2: Writing all partitions...
%PYTHON% -m esptool --chip esp32s3 -p COM7 -b 460800 write_flash 0x0 build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0xd000 build/ota_data_initial.bin 0x10000 build/xiaozhi.bin
if %errorlevel% neq 0 (
    echo FLASH FAILED!
    pause
    exit /b 1
)

echo.
echo Step 3: Verifying factory partition header...
%PYTHON% -m esptool --chip esp32s3 -p COM7 read_flash 0x10000 0x4 build\verify.bin
echo.

echo ============================================
echo  DONE! Press RESET on the board.
echo ============================================
pause
