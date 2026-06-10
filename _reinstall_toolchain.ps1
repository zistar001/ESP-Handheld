$env:MSYSTEM=''
$env:IDF_PATH='D:\code\esp-idf-v5.4.1'
$env:IDF_TOOLS_PATH='C:\Espressif\tools'
$pythonExe = 'C:\Espressif\tools\python_env\idf5.4_py3.12_env\Scripts\python.exe'
$idfToolsPy = "$env:IDF_PATH\tools\idf_tools.py"

Write-Host "=== Force re-download ==="
& $pythonExe $idfToolsPy download xtensa-esp-elf
Write-Host "Download exit: $LASTEXITCODE"

Write-Host "=== Verify ZIP ==="
$zipPath = "C:\Espressif\tools\dist\xtensa-esp-elf-14.2.0_20241119-x86_64-w64-mingw32.zip"
if (Test-Path $zipPath) {
    $zip = [IO.Compression.ZipFile]::OpenRead($zipPath)
    $entries = $zip.Entries | Where-Object { $_.FullName -like "*cc1.exe*" }
    Write-Host "cc1 entries in ZIP: $($entries.Count)"
    if ($entries.Count -gt 0) {
        $entries[0] | Select-Object Name, Length
    }
    $zip.Dispose()
}

Write-Host "=== Install ==="
& $pythonExe $idfToolsPy install xtensa-esp-elf
Write-Host "Install exit: $LASTEXITCODE"

Write-Host "=== Verify cc1 ==="
$cc1Path = "C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20241119\xtensa-esp-elf\libexec\gcc\xtensa-esp-elf\14.2.0\cc1.exe"
if (Test-Path $cc1Path) {
    Write-Host "cc1.exe FOUND at: $cc1Path"
} else {
    Write-Host "cc1.exe NOT FOUND"
    Get-ChildItem -Path "C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20241119" -Recurse -Filter "cc1*" -ErrorAction SilentlyContinue | ForEach-Object { Write-Host "  Found: $($_.FullName)" }
}
