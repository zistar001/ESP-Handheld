# retro-go-firmware build script
# Run: cd retro-go-firmware && powershell -ExecutionPolicy Bypass -File build_rg.ps1
#
# Prerequisites: ESP-IDF v5.5.4 installed with export.ps1 available
#                retro-go emulator cores at the path set in RETRO_GO_CORE_DIR

$env:MSYSTEM = ''

# --- Customize these for your environment ---
$env:IDF_PATH       = $env:IDF_PATH       # Should be set by export.ps1 or install.ps1
$env:IDF_TOOLS_PATH = $env:IDF_TOOLS_PATH
$env:IDF_COMPONENT_MANAGER = '0'
$COM_PORT           = 'COM3'             # Change to your port

# Activate ESP-IDF Python virtual environment
$venv = "$env:USERPROFILE\.espressif\python_env\idf5.5_py3.11_env\Scripts"

# Add IDF tools (cmake, ninja, toolchain) to PATH
$idf_tools = "$env:IDF_TOOLS_PATH\cmake\3.30.2\bin"
$idf_tools += ";$env:IDF_TOOLS_PATH\ninja\1.12.1"
$idf_tools += ";$env:IDF_TOOLS_PATH\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin"
$idf_tools += ";$env:IDF_TOOLS_PATH\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin"

$env:PATH = "$venv;$idf_tools;$env:IDF_PATH\tools;$env:PATH"

if (-not (Test-Path "$venv\python.exe")) {
    Write-Host "ERROR: Python venv not found at $venv"
    Write-Host "Set up ESP-IDF first: cd $env:IDF_PATH && .\install.ps1"
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
if (Test-Path $bin) {
    $size = (Get-Item $bin).Length
    Write-Host "Output: retro-go-firmware.bin ($([math]::Round($size/1MB,2)) MB)"
}
Write-Host ""
Write-Host "=== To flash ==="
Write-Host "python -m esptool --chip esp32s3 -p $COM_PORT -b 921600 write_flash 0x410000 $bin"
