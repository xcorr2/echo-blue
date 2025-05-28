import asyncio
from bleak import BleakScanner

# Position of bytes
flag = 2
gps_bytes = [3, 4, 5, 6, 7, 8]
frequency_byte = 9

# id to check for in any ble advertisment
ble_id = 0x1279

device_queue = asyncio.Queue()

found_event = asyncio.Event()

def detection_callback(device, advertisement_data):
    for company_id, data in advertisement_data.manufacturer_data.items():
        if company_id == ble_id:
            print(f"[{device.address}] {device.name or 'Unknown'}")
            print(f"  Manufacturer ID: 0x{company_id:04X}")
            print(f"  Payload: {data.hex()}\n")

            # flag byte
            flags = data[flag]
            # x and y coordinates
            x = unpack(data[2:5])
            y = unpack(data[5:8])
            # frequency byte
            frequency = data[frequency_byte]

            data = {
                    "flags": flags,
                    "x": x,
                    "y": y,
                    "frequency": frequency
            }
            asyncio.create_task(device_queue.put(data))  # Schedule async put

            # indicate device was found to stop scanning
            #found_event.set()    


# take 3 bytes and convert to decimal position value
def unpack(bytes):
     print(bytes)
     # first byte is value before decimal
     meters = bytes[0]
     # next two bytes combine for value after decimal
     millimeters = bytes[1] | (bytes[2] << 8)
     print(millimeters)
     return (meters + millimeters / 1000)

async def main():
    scanner = BleakScanner()
    scanner.register_detection_callback(detection_callback)

    print("Scanning...")
    # search for ble advertisment
    async with scanner:
        # check for item in queue. If true, end scanning, else continue
        result = await device_queue.get()
        print(result)
    # result found, print values

    print(f"flag: {result['flags']}")
    print(f"x: {result['x']}")
    print(f"y: {result['y']}")
    print(f"frequency: {result['frequency']}")

    flag = result['flags']
    x = result['x']
    y = result['y']
    freq = result['frequency']



asyncio.run(main())
