# Quick build ESP_BSP (home environment)
$env:MSYSTEM = ''
$env:IDF_PATH = 'D:\Espressif\frameworks\esp-idf-v5.5.4'
$env:IDF_TOOLS_PATH = 'D:\Espressif\tools'
$env:IDF_COMPONENT_MANAGER = '1'
$env:IDF_PYTHON_ENV_PATH = 'C:\Users\Administrator\.espressif\python_env\idf5.5_py3.11_env'
$env:ESP_ROM_ELF_DIR = "$env:IDF_PATH\components\esp_rom\esp32s3"

$env:PATH = "C:\Users\Administrator\.espressif\python_env\idf5.5_py3.11_env\Scripts"
$env:PATH += ";D:\Espressif\tools\cmake\3.30.2\bin"
$env:PATH += ";D:\Espressif\tools\ninja\1.12.1"
$env:PATH += ";D:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin"
$env:PATH += ";D:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin"
$env:PATH += ";$env:IDF_PATH\tools"
$env:PATH += ";$env:PATH"

python $env:IDF_PATH\tools\idf.py build
if ($LASTEXITCODE -eq 0) {
    Write-Host "=== BUILD OK ==="
} else {
    Write-Host "=== BUILD FAILED ==="
}
