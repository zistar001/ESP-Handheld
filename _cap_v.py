import serial, time, sys
ser = serial.Serial('COM3', 115200, timeout=1)
# No DTR/RTS — don't reset!
start = time.time()
while time.time() - start < 60:
    line = ser.readline()
    if line:
        t = line.decode('utf-8', errors='replace').rstrip()
        ts = time.time() - start
        print(f'[{ts:5.1f}s] {t}')
        sys.stdout.flush()
ser.close()
print('\n--- DONE ---')
