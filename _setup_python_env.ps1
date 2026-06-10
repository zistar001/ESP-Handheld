$env:MSYSTEM=''
$env:IDF_PATH='D:\code\esp-idf-v5.4.1'
$env:IDF_TOOLS_PATH='C:\Espressif\tools'
$pythonExe = 'C:\Espressif\tools\python\4c2820d\venv\Scripts\python.exe'
$venvPath = 'C:\Espressif\tools\python_env\idf5.4_py3.12_env'

Write-Host "Creating Python venv at: $venvPath"
& $pythonExe -m venv $venvPath
if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to create venv"
    exit 1
}

Write-Host "Installing pip packages..."
$venvPython = "$venvPath\Scripts\python.exe"
& $venvPython -m pip install --upgrade pip
& $venvPython -m pip install -r "$env:IDF_PATH\tools\requirements\requirements.core.txt" --constraint "$env:IDF_TOOLS_PATH\espidf.constraints.v5.4.txt"
Write-Host "Setup complete"
