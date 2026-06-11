"""Send PC IP to ESP32 via BLE GATT characteristic"""
import asyncio, socket, sys
from bleak import BleakClient, BleakScanner

IPC_CHR_UUID = "0000BBBB-0000-1000-8000-00805F9B34FB"

async def send_ip():
    pc_ip = socket.gethostbyname(socket.gethostname())
    # Try to get real IP
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try: s.connect(('8.8.8.8', 1)); pc_ip = s.getsockname()[0]
    except: pass
    finally: s.close()
    print(f"PC IP: {pc_ip}")

    print("Scanning for ESP-Handheld...")
    dev = await BleakScanner.find_device_by_name("ESP-Handheld", timeout=10)
    if not dev:
        print("ESP-Handheld not found! Is BLE paired?")
        return
    print(f"Found: {dev.address}")

    async with BleakClient(dev) as client:
        # Find our custom characteristic
        svc = client.services
        for s in svc.services.values():
            for c in s.characteristics:
                if "0000bbbb" in c.uuid.lower():
                    await client.write_gatt_char(c.uuid, pc_ip.encode())
                    print(f"IP sent to ESP32 via BLE: {pc_ip}")
                    return
        print("IPC characteristic not found on ESP32")

if __name__ == "__main__":
    asyncio.run(send_ip())
