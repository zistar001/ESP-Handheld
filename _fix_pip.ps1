$env:IDF_PATH='D:\code\esp-idf-v5.4.1'
$env:IDF_TOOLS_PATH='C:\Espressif\tools'
$pythonExe = 'C:\Espressif\tools\python_env\idf5.4_py3.12_env\Scripts\python.exe'

Write-Host "Installing packages with v5.4 constraints..."
& $pythonExe -m pip install --upgrade pip
& $pythonExe -m pip install -r "$env:IDF_PATH\tools\requirements\requirements.core.txt" --constraint "$env:IDF_TOOLS_PATH\espidf.constraints.v5.4.txt"
Write-Host "Done"
