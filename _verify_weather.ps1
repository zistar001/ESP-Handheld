$portName = "COM3"
$baud = 115200
try {
    $port = New-Object System.IO.Ports.SerialPort $portName, $baud, None, 8, one
    $port.ReadTimeout = 3000
    $port.Open()
    Start-Sleep -Seconds 30
    $data = $port.ReadExisting()
    if ($data.Length -gt 0) {
        $lines = $data -split "`n" | Where-Object { $_ -match "WEATHER|TIME_SYNC|HTTP|Weather|Fetch" }
        if ($lines.Length -gt 0) {
            Write-Host "=== WEATHER/TIME LOGS ==="
            $lines | ForEach-Object { Write-Host $_ }
            Write-Host "=== END ==="
        } else {
            Write-Host "(no weather/time logs found)"
            Write-Host "=== LAST 20 LINES ==="
            ($data -split "`n") | Select-Object -Last 20 | ForEach-Object { Write-Host $_ }
        }
    } else {
        Write-Host "No data"
    }
    $port.Close()
} catch {
    Write-Host "Error: $($_.Exception.Message)"
}
