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
echo === Flashing xiaozhi.bin to OTA_1 partition (0x810000) ===
echo === Dual-system: XiaoZhi AI in ota_1, Game firmware in factory ===
echo.
%PYTHON% -m esptool --chip esp32s3 -p COM7 -b 921600 write_flash 0x810000 build/xiaozhi.bin
echo.
echo Flash exit code: %errorlevel%
echo === Done! Press RESET to start ===
pause
