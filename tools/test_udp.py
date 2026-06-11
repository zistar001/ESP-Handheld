"""Bare UDP listener - just print received packets"""
import socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('0.0.0.0', 9210))
sock.settimeout(1)
print("Listening UDP:9210 ... (wait 30s)")
print("Your local IP:", socket.gethostbyname(socket.gethostname()))
count = 0
end = __import__('time').time() + 30
while __import__('time').time() < end:
    try:
        data, addr = sock.recvfrom(4096)
        count += 1
        if count <= 5 or count % 20 == 0:
            print(f"Packet #{count} from {addr}, len={len(data)}")
    except socket.timeout:
        pass  # keep waiting
print(f"\nTotal: {count} packets received in 30s")
if count == 0:
    print(">>> PC is NOT receiving UDP from ESP32!")
