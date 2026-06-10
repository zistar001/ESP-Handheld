$env:MSYSTEM=''
$env:IDF_PATH='D:\code\esp-idf-v5.4.1'
$env:IDF_TOOLS_PATH='C:\Espressif\tools'
$env:IDF_COMPONENT_MANAGER=0
$env:IDF_PYTHON_ENV_PATH='C:\Espressif\tools\python_env\idf5.4_py3.12_env'
$env:PYTHON="C:\Espressif\tools\python_env\idf5.4_py3.12_env\Scripts\python.exe"
$env:PATH='C:\Espressif\tools\cmake\3.30.2\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20241119\xtensa-esp-elf\bin;C:\Espressif\tools\python_env\idf5.4_py3.12_env\Scripts;C:\Espressif\tools\ccache\4.12.1-windows-x86_64;' + $env:PATH

Set-Location 'D:\code\Claudecode\ESP_BSP'

Write-Host "=== Flashing to COM3 ==="
& $env:PYTHON $env:IDF_PATH\tools\idf.py -p COM3 flash
Write-Host "Flash exit code: $LASTEXITCODE"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "=== Flash complete ==="
