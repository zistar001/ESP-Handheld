$portName = "COM3"
$baud = 115200

try {
    $port = new-Object System.IO.Ports.SerialPort $portName, $baud, None, 8, one
    $port.ReadTimeout = 3000
    $port.Open()
    Write-Host "=== Reading from $portName (3 seconds) ==="
    $data = ""
    for ($i = 0; $i -lt 30; $i++) {
        try {
            $data += $port.ReadExisting()
        } catch {
            # timeout
        }
        Start-Sleep -Milliseconds 100
    }
    if ($data.Length -gt 0) {
        Write-Host $data
    } else {
        Write-Host "(no data received)"
    }
    $port.Close()
    Write-Host "=== Done ==="
} catch {
    Write-Host "Error: $($_.Exception.Message)"
}
