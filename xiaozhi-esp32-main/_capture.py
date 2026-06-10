import serial
import sys
import time

port = sys.argv[1] if len(sys.argv) > 1 else "COM7"
baud = 115200
timeout_sec = 8

ser = serial.Serial(port, baud, timeout=1)
ser.dtr = False
ser.rts = True
time.sleep(0.1)
ser.rts = False
time.sleep(0.1)

start = time.time()
lines = []
while time.time() - start < timeout_sec:
    line = ser.readline()
    if line:
        try:
            text = line.decode('utf-8', errors='replace').rstrip()
        except:
            text = str(line)
        lines.append(text)
        print(text)

ser.close()
