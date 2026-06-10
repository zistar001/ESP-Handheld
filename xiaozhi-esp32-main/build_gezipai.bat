@echo off
REM Strip MSYS2 env vars that break ESP-IDF builds
set MSYSTEM=
set MINGW_PREFIX=
set MINGW_CHOST=
set MSYS2_PATH=
set MSYSCON=
set IDF_PATH=D:\Espressif\frameworks\esp-idf-v5.5.4
set IDF_TOOLS_PATH=D:\Espressif\tools
set IDF_PYTHON_ENV_PATH=D:\Espressif\tools\python_env\idf5.5_py3.11_env
REM Enable component manager for fetching dependencies (esp-sr, xiaozhi-fonts, etc.)
set IDF_COMPONENT_MANAGER=1
set PYTHON=C:\Users\Administrator\.espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe
set PATH=D:\Espressif\tools\cmake\3.30.2\bin;D:\Espressif\tools\ninja\1.12.1;D:\Espressif\tools\idf-git\2.44.0\cmd;D:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin;C:\Users\Administrator\.espressif\python_env\idf5.5_py3.11_env\Scripts;%PATH%
cd /d D:\code\claudecode\ESP_BSP\xiaozhi-esp32-main
REM Apply gezipai board defaults (overrides base + esp32s3 defaults)
set SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.esp32s3;sdkconfig.defaults.gezipai
echo === Setting target esp32s3 ===
%PYTHON% %IDF_PATH%\tools\idf.py set-target esp32s3
echo === set-target exit code: %errorlevel% ===
if %errorlevel% neq 0 goto :end
echo === Building gezipai board ===
%PYTHON% %IDF_PATH%\tools\idf.py -DCONFIG_BOARD_TYPE_GEZIPAI=y build
echo === Build exit code: %errorlevel% ===
:end
pause
