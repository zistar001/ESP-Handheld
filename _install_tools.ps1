$env:MSYSTEM=''
$env:IDF_PATH='D:\code\esp-idf-v5.4.1'
$env:IDF_TOOLS_PATH='C:\Espressif\tools'
$pythonExe = 'C:\Espressif\tools\python_env\idf5.4_py3.12_env\Scripts\python.exe'
$idfToolsPy = "$env:IDF_PATH\tools\idf_tools.py"

Write-Host "=== Listing installed tools ==="
& $pythonExe $idfToolsPy list
Write-Host ""

Write-Host "=== Installing xtensa-esp-elf ==="
& $pythonExe $idfToolsPy install xtensa-esp-elf
Write-Host "Done, exit: $LASTEXITCODE"
