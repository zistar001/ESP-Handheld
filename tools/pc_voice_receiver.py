"""
ESP32 Voice Receiver — auto-discovers ESP32 via UDP, receives PCM audio → VB-Cable
"""
import socket, sys, time, threading, struct
import sounddevice as sd
import numpy as np

PORT = 9210
DISCOVERY_PORT = 9211
ESP_SR = 16000
DEVICE_ID = 16  # CABLE Input @ 44100Hz

# ── Get PC's local IP ──
def get_local_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(('8.8.8.8', 1))
        ip = s.getsockname()[0]
    except: ip = '127.0.0.1'
    finally: s.close()
    return ip

pc_ip = get_local_ip()
print(f"PC IP: {pc_ip}")

# ── Send PC IP to ESP32 discovery port (scan subnet, broadcast blocked) ──
def send_discovery():
    parts = pc_ip.split('.')
    prefix = f"{parts[0]}.{parts[1]}.{parts[2]}."
    dsock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    dsock.settimeout(0.005)
    # Also try broadcast first
    dsock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    dsock.sendto(pc_ip.encode(), (f"{prefix}255", DISCOVERY_PORT))
    # Then scan subnet (1-254)
    print(f"Scanning {prefix}1-254 for ESP32...")
    for i in range(1, 255):
        try: dsock.sendto(pc_ip.encode(), (f"{prefix}{i}", DISCOVERY_PORT))
        except: pass
    dsock.close()
    print(f"Discovery sent to all {prefix}*")

send_discovery()

# ── Audio receive + VB-Cable output ──
ring = bytearray()
lock = threading.Lock()
total = 0

def callback(outdata, frames, time_info, status):
    global ring, total
    needed = frames * 2
    with lock:
        if len(ring) < needed:
            outdata.fill(0); return
        chunk = ring[:needed]; ring = ring[needed:]
        outdata[:, 0] = np.frombuffer(chunk, dtype=np.int16)
        total += needed

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('0.0.0.0', PORT))
sock.settimeout(0.1)
print(f"Listening UDP:{PORT} → CABLE Input [{DEVICE_ID}]")

def udp_rx():
    global ring
    while True:
        try:
            data, addr = sock.recvfrom(2048)
            with lock: ring.extend(data)
        except socket.timeout: pass

threading.Thread(target=udp_rx, daemon=True).start()

try:
    with sd.OutputStream(samplerate=ESP_SR, channels=1, dtype='int16',
                          device=DEVICE_ID, callback=callback, blocksize=256):
        print("Ready! Hold A on ESP32 to speak.")
        while True:
            time.sleep(2)
            if total > 0:
                print(f"  {total//1024} KB streamed", end='\r')
except KeyboardInterrupt:
    print(f"\nTotal: {total//1024} KB")
