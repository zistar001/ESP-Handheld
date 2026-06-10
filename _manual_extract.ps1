$env:MSYSTEM=''
$env:IDF_PATH='D:\code\esp-idf-v5.4.1'
$env:IDF_TOOLS_PATH='C:\Espressif\tools'

$zipPath = "C:\Espressif\tools\dist\xtensa-esp-elf-14.2.0_20241119-x86_64-w64-mingw32.zip"
$targetDir = "C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20241119"

# Check zip exists and its size
if (Test-Path $zipPath) {
    $zipItem = Get-Item $zipPath
    Write-Host "ZIP size: $($zipItem.Length) bytes"
} else {
    Write-Host "ZIP not found, need to download"
    & "C:\Espressif\tools\python_env\idf5.4_py3.12_env\Scripts\python.exe" "$env:IDF_PATH\tools\idf_tools.py" download xtensa-esp-elf
}

Write-Host "Adding System.IO.Compression assembly..."
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

Write-Host "Checking ZIP for cc1..."
try {
    $zip = [System.IO.Compression.ZipFile]::OpenRead($zipPath)
    $cc1Entries = $zip.Entries | Where-Object { $_.FullName -like "*cc1.exe*" }
    Write-Host "cc1 entries in ZIP: $($cc1Entries.Count)"
    $zip.Dispose()
} catch {
    Write-Host "Error opening zip: $_"
}

Write-Host "Extracting ZIP..."
Remove-Item -Recurse -Force $targetDir -ErrorAction SilentlyContinue
[System.IO.Compression.ZipFile]::ExtractToDirectory($zipPath, $targetDir)

Write-Host "Checking for cc1..."
$foundCc1 = Get-ChildItem -Path $targetDir -Recurse -Filter "cc1.exe" -ErrorAction SilentlyContinue
if ($foundCc1) {
    Write-Host "cc1.exe FOUND: $($foundCc1.FullName)"
} else {
    Write-Host "cc1.exe NOT FOUND after extraction"
}

Write-Host "Done"
