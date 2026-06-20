"""
ESP32 Voice Receiver — UDP PCM → VB-Cable + system tray icon
No numpy, pystray on main thread for reliable tray icon.
"""
import socket, sys, time, threading, io, ctypes
import sounddevice as sd

PORT = 9210
ESP_SR = 16000

def find_cable():
    for i, d in enumerate(sd.query_devices()):
        if 'CABLE Input' in d['name'] and 'VB-Audio' in d['name'] and d['max_output_channels'] > 0:
            return i, d['name']
    return None, None

idx, name = find_cable()
if idx is None:
    ctypes.windll.user32.MessageBoxW(0, "VB-Cable not found!\nInstall from https://vb-audio.com/Cable/", "ESP Voice Receiver", 0)
    sys.exit(1)

# Get PC IP
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
try: s.connect(('8.8.8.8', 1)); pc_ip = s.getsockname()[0]
except: pc_ip = '?.?.?.?'
finally: s.close()

log = io.StringIO()
def log_msg(msg):
    t = time.strftime("%H:%M:%S")
    print(f"[{t}] {msg}"); log.write(f"[{t}] {msg}\n")

log_msg(f"PC IP: {pc_ip}  CABLE: [{idx}] {name}  Listening UDP:{PORT}")

# Broadcast PC IP to ESP32
parts = pc_ip.split('.')
if len(parts) == 4:
    try:
        ds = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        ds.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        ds.sendto(pc_ip.encode(), (f"{parts[0]}.{parts[1]}.{parts[2]}.255", 9211))
        ds.close()
    except: pass

# Audio state
ring = bytearray()
lock = threading.Lock()
total_bytes = 0
audio_running = True

def audio_callback(outdata, frames, time_info, st):
    global ring, total_bytes
    needed = frames * 2
    with lock:
        if len(ring) < needed:
            outdata[:] = b'\x00' * needed
            return
        chunk = ring[:needed]
        ring = ring[needed:]
        outdata[:] = chunk
        total_bytes += needed

def audio_thread():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('0.0.0.0', PORT))
    sock.settimeout(0.5)
    while audio_running:
        try:
            data, addr = sock.recvfrom(2048)
            with lock: ring.extend(data)
        except socket.timeout: pass
        except: break
    sock.close()

# Start audio RX thread
th = threading.Thread(target=audio_thread, daemon=True)
th.start()

# Audio output stream
stream = sd.RawOutputStream(samplerate=ESP_SR, channels=1, dtype='int16',
                             device=idx, callback=audio_callback, blocksize=256)
stream.start()

log_msg("Ready - Hold RIGHT on ESP32 to speak")

# ── Tray icon (main thread) ──
try:
    import pystray
    from PIL import Image, ImageDraw

    img = Image.new('RGB', (32, 32), (0, 100, 0))
    d = ImageDraw.Draw(img)
    d.ellipse([4, 4, 28, 28], fill=(0, 200, 0))
    d.rectangle([13, 8, 19, 24], fill=(0, 100, 0))

    def show_log(icon, item=None):
        import tempfile, os
        tmp = os.path.join(tempfile.gettempdir(), "esp_voice_status.txt")
        with open(tmp, 'w') as f: f.write(log.getvalue())
        os.startfile(tmp)

    def on_exit():
        global audio_running
        audio_running = False
        stream.stop()
        icon.stop()

    menu = pystray.Menu(
        pystray.MenuItem(f"PC: {pc_ip}  CABLE: {name}", None, enabled=False),
        pystray.MenuItem("Status", show_log, default=True),
        pystray.MenuItem("Exit", on_exit),
    )

    icon = pystray.Icon("esp_voice", img, "ESP32 Voice Receiver", menu)
    icon.run()  # blocks — runs message loop on main thread

except Exception as e:
    log_msg(f"Tray error: {e}")
    # Keep running without tray
    while True:
        time.sleep(5)
        if total_bytes > 1024:
            log_msg(f"Streamed {total_bytes//1024} KB")
            total_bytes = 0
