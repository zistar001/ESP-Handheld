"""
ESP32 Voice Receiver — UDP PCM → VB-Cable virtual mic + system tray icon
Double-click tray icon to show/hide status window.
"""
import socket, sys, time, threading, io
import sounddevice as sd
import numpy as np

PORT = 9210
ESP_SR = 16000
DEVICE_ID = 16  # CABLE Input @ 44100Hz

# ── Find VB-Cable ──
def find_cable():
    for i, d in enumerate(sd.query_devices()):
        if 'CABLE Input' in d['name'] and 'VB-Audio' in d['name'] and d['max_output_channels'] > 0:
            return i, d['name']
    return None, None

idx, name = find_cable()
if idx is None:
    print("ERROR: CABLE Input not found! Install VB-Cable.")
    input("Press Enter to exit...")
    sys.exit(1)

# ── Get PC IP ──
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
try: s.connect(('8.8.8.8', 1)); pc_ip = s.getsockname()[0]
except: pc_ip = '?.?.?.?'
finally: s.close()

# ── Status log ──
log = io.StringIO()
def status(msg):
    t = time.strftime("%H:%M:%S")
    line = f"[{t}] {msg}"
    print(line); log.write(line + "\n")

status(f"PC IP: {pc_ip}")
status(f"CABLE: [{idx}] {name}")
status(f"Listening UDP:{PORT}...")

# ── Tray icon ──
try:
    import pystray
    from PIL import Image, ImageDraw

    def tray_image():
        img = Image.new('RGB', (32, 32), (0, 100, 0))
        d = ImageDraw.Draw(img)
        d.ellipse([4, 4, 28, 28], fill=(0, 200, 0))
        d.rectangle([13, 8, 19, 24], fill=(0, 100, 0))
        return img

    tray = pystray.Icon("esp_voice", tray_image(), "ESP32 Voice Receiver")
    tray.visible = True

    def show_log(icon, item=None):
        import subprocess, tempfile, os
        tmp = os.path.join(tempfile.gettempdir(), "esp_voice_status.txt")
        with open(tmp, 'w') as f: f.write(log.getvalue())
        subprocess.Popen(['notepad', tmp])

    tray.menu = pystray.Menu(
        pystray.MenuItem(f"PC: {pc_ip}", None, enabled=False),
        pystray.MenuItem("Status", show_log, default=True),
        pystray.MenuItem("Exit", lambda: (sd.stop(), tray.stop())),
    )
    threading.Thread(target=tray.run, daemon=True).start()
    status("Tray icon active - double-click for status")
except ImportError:
    status("pystray not installed, running without tray")

# ── Send PC IP to ESP32 ──
parts = pc_ip.split('.')
if len(parts) == 4:
    prefix = f"{parts[0]}.{parts[1]}.{parts[2]}."
    dsock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    dsock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    dsock.sendto(pc_ip.encode(), (f"{prefix}255", 9211))
    dsock.close()

# ── Audio streaming ──
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
                          device=idx, callback=callback, blocksize=256):
        status("Ready - Hold A on ESP32 to speak")
        while True:
            time.sleep(5)
            if total > 1024:
                status(f"Streamed {total//1024} KB")
                total = 0
except KeyboardInterrupt:
    status("Stopped")
except Exception as e:
    status(f"Error: {e}")
    input("Press Enter to exit...")
