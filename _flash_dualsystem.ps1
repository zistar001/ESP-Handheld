# 双系统刷写：ESP_BSP (factory) + XiaoZhi AI (ota_1)
# 搬家后：只需修改 idf_env.ps1 中的路径

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $scriptDir "idf_env.ps1")

$env:MSYSTEM=''
$env:IDF_PATH=$script:IDF_PATH
$env:IDF_TOOLS_PATH=$script:IDF_TOOLS_PATH
$PY = Join-Path $script:IDF_TOOLS_PATH "python\v5.5.4\venv\Scripts\python.exe"
$IDF_PY = Join-Path $script:IDF_PATH "tools\idf.py"
$ESPTOOL = "$PY -m esptool --chip esp32s3 -p $script:COM_PORT -b 921600 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m"

Write-Host "============================================"
Write-Host "Dual-System Flash: ESP_BSP + XiaoZhi AI"
Write-Host "============================================"

# Build ESP_BSP
Write-Host "`n[1/2] Building ESP_BSP..."
Set-Location $scriptDir
$p = Start-Process -FilePath $PY -ArgumentList "$IDF_PY build" -NoNewWindow -Wait -PassThru
if ($p.ExitCode -ne 0) { Write-Host "ESP_BSP BUILD FAILED"; exit 1 }
Write-Host "ESP_BSP build OK"

# Flash everything
Write-Host "`n[2/2] Flashing..."
Invoke-Expression "$ESPTOOL `
    0x0 build/bootloader/bootloader.bin `
    0x8000 build/partition_table/partition-table.bin `
    0xD000 build/ota_data_initial.bin `
    0x10000 build/esp_handheld.bin `
    0x810000 xiaozhi-esp32-main/build/xiaozhi.bin"

Write-Host "`nDual-system flash complete!"
Write-Host "- Factory (ESP_BSP): 0x10000"
Write-Host "- OTA_1  (XiaoZhi):  0x810000"
Write-Host "`nDevice will boot from factory (ESP_BSP)."
Write-Host "Press RESET to start."
