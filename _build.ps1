$env:MSYSTEM=''
$env:IDF_PATH='D:\code\esp-idf-v5.4.1'
$env:IDF_TOOLS_PATH='C:\Espressif\tools'
$env:IDF_COMPONENT_MANAGER=1
$env:PATH='C:\Espressif\tools\cmake\3.30.2\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20241119\xtensa-esp-elf\bin;C:\Espressif\tools\python_env\idf5.4_py3.12_env\Scripts;' + $env:PATH
Set-Location 'D:\code\Claudecode\ESP_BSP'
Write-Host "=== Building with IDF v5.4.1 (COMPONENT_MANAGER=1) ==="
& 'C:\Espressif\tools\python_env\idf5.4_py3.12_env\Scripts\python.exe' 'D:\code\esp-idf-v5.4.1\tools\idf.py' build
Write-Host "Build exit code: $LASTEXITCODE"
