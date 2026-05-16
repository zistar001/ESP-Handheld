@echo off
REM Do NOT set MSYSTEM here - this batch file runs via PowerShell which avoids the MSYS2 env var injection issue
set IDF_PATH=D:\Espressif\frameworks\esp-idf-v5.5.4
set IDF_TOOLS_PATH=D:\Espressif\tools
set IDF_PYTHON_ENV_PATH=D:\Espressif\tools\python_env\idf5.5_py3.11_env
set IDF_COMPONENT_MANAGER=0
set PYTHON=C:\Users\Administrator\.espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe
set PATH=D:\Espressif\tools\cmake\3.30.2\bin;D:\Espressif\tools\ninja\1.12.1;D:\Espressif\tools\idf-git\2.44.0\cmd;D:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin;C:\Users\Administrator\.espressif\python_env\idf5.5_py3.11_env\Scripts;%PATH%
cd /d D:\code\claudecode\ESP_BSP
echo Building...
%PYTHON% %IDF_PATH%\tools\idf.py build
echo Build exit code: %errorlevel%
pause
