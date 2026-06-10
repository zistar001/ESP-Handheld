$portName = "COM3"
$baud = 115200

try {
    $port = new-Object System.IO.Ports.SerialPort $portName, $baud, None, 8, one
    $port.ReadTimeout = 5000
    $port.Open()
    Write-Host "=== Serial monitor (waiting 20s for game FPS data) ==="
    $data = ""
    for ($i = 0; $i -lt 200; $i++) {
        try {
            $chunk = $port.ReadExisting()
            if ($chunk.Length -gt 0) {
                $data += $chunk
                # Print live output
                Write-Host -NoNewline $chunk
            }
        } catch {
            # timeout
        }
        Start-Sleep -Milliseconds 100
    }
    $port.Close()
    Write-Host ""
    Write-Host "=== Done ==="
} catch {
    Write-Host "Error: $($_.Exception.Message)"
}
