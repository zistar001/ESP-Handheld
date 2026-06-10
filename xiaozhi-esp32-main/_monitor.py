import serial, time
ser = serial.Serial('COM7', 115200, timeout=1)
ser.dtr = False
ser.rts = True
time.sleep(0.1)
ser.rts = False
start = time.time()
while time.time() - start < 12:
    line = ser.readline()
    if line:
        print(line.decode('utf-8', errors='replace').rstrip())
ser.close()
