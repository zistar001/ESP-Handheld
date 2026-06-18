param([switch]$Flash)

# Set up home environment
$env:MSYSTEM = ''
$env:IDF_PATH = 'D:\Espressif\frameworks\esp-idf-v5.5.4'
$env:IDF_TOOLS_PATH = 'D:\Espressif\tools'
$env:IDF_COMPONENT_MANAGER = '1'
$PY = 'D:\Espressif\tools\python_env\idf5.5_py3.11_env\Scripts\python.exe'
$env:Path = "D:\Espressif\tools\cmake\3.30.2\bin;$env:Path"
$env:Path = "D:\Espressif\tools\ninja\1.12.1;$env:Path"
$env:Path = "D:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin;$env:Path"
$env:Path = "D:\Espressif\tools\python_env\idf5.5_py3.11_env\Scripts;$env:Path"
$env:Path = "D:\Espressif\frameworks\esp-idf-v5.5.4\tools;$env:Path"

Set-Location D:\code\claudecode\ESP_BSP

Write-Host "=== BUILDING ESP_BSP (home environment) ==="
Write-Host "Python: $PY"
Write-Host "IDF_PATH: $env:IDF_PATH"
Write-Host ""

# Clean build
if (Test-Path build) {
    Remove-Item -Recurse -Force build
    Write-Host "Clean: old build removed"
}

# Build
$p = Start-Process -FilePath $PY -ArgumentList "$env:IDF_PATH\tools\idf.py build" -NoNewWindow -Wait -PassThru
if ($p.ExitCode -ne 0) {
    Write-Host "=== BUILD FAILED (exit code $($p.ExitCode)) ==="
    exit 1
}

# Verify
$fw = "D:\code\claudecode\ESP_BSP\build\esp_handheld.bin"
if (Test-Path $fw) {
    $size = (Get-Item $fw).Length
    Write-Host "=== BUILD SUCCESS: $size bytes ==="
} else {
    Write-Host "=== BUILD FAILED: firmware not found ==="
    exit 1
}

# Flash if requested
if ($Flash) {
    Write-Host ""
    Write-Host "=== FLASHING to COM7 ==="
    & $PY -m esptool --chip esp32s3 -p COM7 -b 921600 `
        --before default_reset --after hard_reset write_flash `
        --flash_mode dio --flash_size 16MB --flash_freq 80m `
        0x0 build/bootloader/bootloader.bin `
        0x8000 build/partition_table/partition-table.bin `
        0xd000 build/ota_data_initial.bin `
        0x10000 build/esp_handheld.bin
    if ($LASTEXITCODE -eq 0) {
        Write-Host "=== FLASH SUCCESS ==="
    } else {
        Write-Host "=== FLASH FAILED ==="
    }
}
