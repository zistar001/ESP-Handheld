$portName = "COM3"
$baud = 115200
try {
    $port = New-Object System.IO.Ports.SerialPort $portName, $baud, None, 8, one
    $port.ReadTimeout = 3000
    $port.Open()
    Start-Sleep -Seconds 15
    $data = $port.ReadExisting()
    if ($data.Length -gt 0) {
        Write-Host "=== SERIAL OUTPUT ==="
        Write-Host $data
        Write-Host "=== END ==="
    } else {
        Write-Host "No data received"
    }
    $port.Close()
} catch {
    Write-Host "Error: $($_.Exception.Message)"
}
