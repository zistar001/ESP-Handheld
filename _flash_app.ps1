$env:MSYSTEM=''
$env:PATH='C:\Espressif\tools\python_env\idf5.4_py3.12_env\Scripts;' + $env:PATH
Set-Location 'D:\code\Claudecode\ESP_BSP'
Write-Host "=== Flashing app to COM3 ==="
python -m esptool --chip esp32s3 -p COM3 -b 921600 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m 0x10000 build\esp_handheld.bin
Write-Host "Flash exit code: $LASTEXITCODE"
