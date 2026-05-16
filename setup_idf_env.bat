@echo off
set IDF_PATH=D:\Espressif\frameworks\esp-idf-v5.5.4
set IDF_TOOLS_PATH=D:\Espressif\tools
set PYTHON=D:\Espressif\tools\idf-python\3.11.2\python.exe
%PYTHON% %IDF_PATH%\tools\idf_tools.py install-python-env
if %errorlevel% neq 0 (
    echo install-python-env failed, trying direct venv creation...
    %PYTHON% -m venv %IDF_PATH%\python_env\idf5.5_py3.11_env
    call %IDF_PATH%\python_env\idf5.5_py3.11_env\Scripts\activate.bat
    python.exe -m pip install --no-index --find-links %IDF_TOOLS_PATH%\idf-python-wheels\3.11-2023-03-05 -r %IDF_PATH%\tools\requirements\requirements.core.txt
    if %errorlevel% neq 0 (
        echo Warning: Pip install failed, will try direct idf.py
    )
)
echo Done
pause
