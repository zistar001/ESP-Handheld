$env:MSYSTEM=''
$env:IDF_PATH='D:\Espressif\frameworks\esp-idf-v5.5.4'
$env:IDF_TOOLS_PATH='D:\Espressif\tools'
$env:IDF_COMPONENT_MANAGER='0'
$env:PATH='D:\Espressif\tools\cmake\3.30.2\bin;D:\Espressif\tools\ninja\1.12.1;D:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin;C:\Users\Administrator\.espressif\python_env\idf5.5_py3.11_env\Scripts;' + $env:PATH
Set-Location D:\code\claudecode\ESP_BSP
$p = Start-Process -FilePath 'C:\Users\Administrator\.espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe' -ArgumentList 'D:\Espressif\frameworks\esp-idf-v5.5.4\tools\idf.py','build' -NoNewWindow -Wait -PassThru
if ($p.ExitCode -ne 0) { Write-Host "BUILD FAILED"; exit 1 }
C:\Users\Administrator\.espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe -m esptool --chip esp32s3 -p COM7 -b 921600 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m 0x0 build\bootloader\bootloader.bin 0x8000 build\partition_table\partition-table.bin 0xd000 build\ota_data_initial.bin 0x10000 build\esp_handheld.bin
Write-Host "Done: $LASTEXITCODE"
