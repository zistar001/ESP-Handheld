@echo off
set MSYSTEM=
set MINGW_PREFIX=
set MINGW_CHOST=
set MSYS2_PATH=
set MSYSCON=
set IDF_PATH=D:\Espressif\frameworks\esp-idf-v5.5.4
set IDF_TOOLS_PATH=D:\Espressif\tools
set IDF_PYTHON_ENV_PATH=D:\Espressif\tools\python_env\idf5.5_py3.11_env
set PYTHON=C:\Users\Administrator\.espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe
set PATH=D:\Espressif\tools\cmake\3.30.2\bin;D:\Espressif\tools\ninja\1.12.1;D:\Espressif\tools\idf-git\2.44.0\cmd;D:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin;D:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\libexec\gcc\xtensa-esp-elf\14.2.0;C:\Users\Administrator\.espressif\python_env\idf5.5_py3.11_env\Scripts;C:\windows\system32;C:\windows;C:\windows\System32\Wbem;%PATH%
cd /d D:\code\claudecode\ESP_BSP\xiaozhi-esp32-main

echo === Reading partition table from flash ===
%PYTHON% -m esptool --chip esp32s3 -p COM7 read_flash 0x8000 0x1000 build\partition_table_readback.bin
echo.

echo === Reading ota_data (0xD000, 8KB) ===
%PYTHON% -m esptool --chip esp32s3 -p COM7 read_flash 0xD000 0x2000 build\ota_data_readback.bin
echo.

echo === Checking factory partition header (0x10000) ===
%PYTHON% -m esptool --chip esp32s3 -p COM7 read_flash 0x10000 0x20 build\factory_header.bin
echo.

echo === Checking ota_1 partition header (0x810000) ===
%PYTHON% -m esptool --chip esp32s3 -p COM7 read_flash 0x810000 0x20 build\ota1_header.bin
echo.

echo === Done ===
