# retro-go-firmware build script
# Run: cd retro-go-firmware && powershell -ExecutionPolicy Bypass -File build_rg.ps1

$env:MSYSTEM = ''

# --- Environment (Home PC) ---
$env:IDF_PATH       = 'D:\Espressif\frameworks\esp-idf-v5.5.4'
$env:IDF_TOOLS_PATH = 'D:\Espressif\tools'
$env:IDF_COMPONENT_MANAGER = '0'
$env:IDF_PYTHON_ENV_PATH = "$env:USERPROFILE\.espressif\python_env\idf5.5_py3.11_env"
$env:ESP_ROM_ELF_DIR = "$env:IDF_PATH\components\esp_rom\esp32s3"
$COM_PORT           = 'COM8'

# Activate ESP-IDF Python virtual environment
$venv = "$env:USERPROFILE\.espressif\python_env\idf5.5_py3.11_env\Scripts"

# Add IDF tools (cmake, ninja, toolchain) to PATH
$idf_tools = "$env:IDF_TOOLS_PATH\cmake\3.30.2\bin"
$idf_tools += ";$env:IDF_TOOLS_PATH\ninja\1.12.1"
$idf_tools += ";$env:IDF_TOOLS_PATH\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin"
$idf_tools += ";$env:IDF_TOOLS_PATH\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin"
# No ULP toolchain needed for retro-go

$env:PATH = "$venv;$idf_tools;$env:IDF_PATH\tools;$env:PATH"

if (-not (Test-Path "$venv\python.exe")) {
    Write-Host "ERROR: Python venv not found at $venv"
    exit 1
}

Set-Location $PSScriptRoot

# Set target (first time only)
if (-not (Test-Path "sdkconfig")) {
    Write-Host "=== Setting target esp32s3 ==="
    python $env:IDF_PATH\tools\idf.py set-target esp32s3
    if ($LASTEXITCODE -ne 0) { Write-Host "FAILED"; exit 1 }
}

# Build
Write-Host "=== Building retro-go-firmware ==="
python $env:IDF_PATH\tools\idf.py build
if ($LASTEXITCODE -ne 0) { Write-Host "BUILD FAILED"; exit 1 }

# Report
Write-Host "=== Build complete ==="
$bin = "$PSScriptRoot\build\retro-go-firmware.bin"
$boot = "$PSScriptRoot\build\bootloader\bootloader.bin"
$ptable = "$PSScriptRoot\build\partition_table\partition-table.bin"
if (Test-Path $bin) {
    $size = (Get-Item $bin).Length
    Write-Host "Output: retro-go-firmware.bin ($([math]::Round($size/1MB,2)) MB)"
    Write-Host "72% free in 4MB partition — plenty of room!"
}
Write-Host ""
Write-Host "=== Flashing ==="
Write-Host ""
Write-Host "1) Full flash (bootloader + firmware + partition table to ota_0):"
Write-Host "   cd $(Split-Path $PSScriptRoot -Parent)"
Write-Host "   .\_bf.ps1  (backs up current firmware)"
Write-Host ""
Write-Host "2) Flash retro-go ONLY to ota_0 (keeps ESP_BSP on factory):"
Write-Host "   $PY -m esptool --chip esp32s3 -p $COM_PORT -b 921600 write_flash 0x410000 $bin"
Write-Host ""
Write-Host "3) After flashing, switch to retro-go via ESP_BSP menu or:"
Write-Host "   $PY -m esptool --chip esp32s3 -p $COM_PORT -b 921600 erase_region 0xd000 0x2000"
Write-Host "   (Erases OTA data — bootloader boots the first valid app = ota_0)"
Write-Host ""
Write-Host "4) To return to ESP_BSP:"
Write-Host "   Re-flash ota_data_initial.bin to 0xd000 (from ESP_BSP project)"
