# Build + Flash ESP_BSP (factory partition)
# 搬家后：只需修改 idf_env.ps1 中的路径

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $scriptDir "idf_env.ps1")

$env:MSYSTEM=''
$env:IDF_PATH=$script:IDF_PATH
$env:IDF_TOOLS_PATH=$script:IDF_TOOLS_PATH
$env:IDF_COMPONENT_MANAGER='0'
$PY = Join-Path $script:IDF_TOOLS_PATH "idf-python\3.11.2\python.exe"
$IDF_PY = Join-Path $script:IDF_PATH "tools\idf.py"
$ESPTOOL = "$PY -m esptool --chip esp32s3 -p $script:COM_PORT -b 921600 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m"

Set-Location $scriptDir

$p = Start-Process -FilePath $PY -ArgumentList "$IDF_PY build" -NoNewWindow -Wait -PassThru
if ($p.ExitCode -ne 0) { Write-Host "BUILD FAILED"; exit 1 }

& cmd /c "$ESPTOOL 0x0 build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0xd000 build/ota_data_initial.bin 0x10000 build/esp_handheld.bin"
Write-Host "Done: $LASTEXITCODE"
