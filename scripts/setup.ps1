# ESP-Handheld — Windows Setup & Build Script
# ============================================
# This script guides you through setting up and building the firmware.
#
# Prerequisites:
#   - ESP-IDF v5.5.4 installed (see https://docs.espressif.com/projects/esp-idf/)
#   - Python 3.11+
#   - Git
#
# Usage:
#   .\scripts\setup.ps1              # Setup + build
#   .\scripts\setup.ps1 -Build       # Build only (skip submodule init)
#   .\scripts\setup.ps1 -Menuconfig  # Open menuconfig after setup

param(
    [switch]$Menuconfig,
    [switch]$Build
)

$ErrorActionPreference = "Stop"
$ROOT = Split-Path -Parent $PSScriptRoot

Write-Host "=== ESP-Handheld Firmware Setup ===" -ForegroundColor Cyan

# 1. Check ESP-IDF
if (-not $env:IDF_PATH) {
    # Try common locations
    $candidates = @(
        "$env:USERPROFILE\esp\esp-idf",
        "C:\Espressif\frameworks\esp-idf-v5.5.4",
        "C:\esp-idf"
    )
    foreach ($p in $candidates) {
        if (Test-Path "$p\export.ps1") {
            $env:IDF_PATH = $p
            Write-Host "Found ESP-IDF at: $p" -ForegroundColor Green
            break
        }
    }
    if (-not $env:IDF_PATH) {
        Write-Host "ESP-IDF not found. Please install it first:" -ForegroundColor Yellow
        Write-Host "  https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/"
        Write-Host ""
        Write-Host "Then run this script from an ESP-IDF PowerShell environment."
        exit 1
    }
}

# 2. Init submodules (skip if -Build only)
if (-not $Build) {
    Write-Host "=== Initializing submodules ===" -ForegroundColor Cyan
    Set-Location $ROOT
    git submodule update --init --recursive
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Submodule init failed." -ForegroundColor Red
        exit 1
    }
}

# 3. Set environment
$env:MSYSTEM = ''
$env:IDF_COMPONENT_MANAGER = '0'

Set-Location $ROOT

# 4. Open menuconfig if requested
if ($Menuconfig) {
    Write-Host "=== Opening menuconfig ===" -ForegroundColor Cyan
    Write-Host "Set your Weather API Key in: ESP-Handheld Configuration → Weather API Key"
    python $env:IDF_PATH\tools\idf.py menuconfig
    if ($LASTEXITCODE -ne 0) { exit 1 }
}

# 5. Build
Write-Host "=== Building ===" -ForegroundColor Cyan
python $env:IDF_PATH\tools\idf.py build
if ($LASTEXITCODE -ne 0) {
    Write-Host "BUILD FAILED" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "=== Build complete! ===" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:"
Write-Host "  1) Connect your ESP32-S3 via USB"
Write-Host "  2) Full flash: python -m esptool --chip esp32s3 -p COM3 -b 921600"
Write-Host "     --before default_reset --after hard_reset write_flash"
Write-Host "     --flash_mode dio --flash_size 16MB --flash_freq 80m"
Write-Host "     0x0 build/bootloader/bootloader.bin"
Write-Host "     0x8000 build/partition_table/partition-table.bin"
Write-Host "     0xd000 build/ota_data_initial.bin"
Write-Host "     0x10000 build/esp_handheld.bin"
Write-Host ""
Write-Host "  3) App-only: python -m esptool --chip esp32s3 -p COM3 -b 921600"
Write-Host "     write_flash 0x10000 build/esp_handheld.bin"
