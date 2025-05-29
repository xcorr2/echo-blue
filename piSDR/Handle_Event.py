import requests
import json
import threading
import time
import signal
import subprocess
import asyncio
from bleak import BleakScanner

piURL = 'https://api.us-e1.tago.io/data'
piHeaderGET = {
    'device-token': 'API KEY'
}
piHeaders = {
    'device-token': 'DEVICE ID',
    'Content-Type': 'application/json'
}

scan_stop_event = threading.Event() # Create a stop event

def get_SDR_params():
    requestURL = "https://api.us-e1.tago.io/data?variables[]=frequency&variables[]=seconds&variables[]=saveMP3&variables[]=emergency&query=last_item"
    responseGET = requests.request("GET", requestURL, headers=piHeaderGET)
    if responseGET.status_code == 200:
        responseGET = responseGET.json()
        frequency = responseGET["result"][0]["value"]
        seconds = responseGET["result"][1]["value"]
        saveMP3 = responseGET["result"][2]["value"]
        emergency = responseGET["result"][3]["value"]
        return frequency, seconds, saveMP3, emergency
    else:
        print("Error retrieving recording params from TagoIO")
        exit()

def upload_GPS(latitude, longitude):
    gpsData = [
        {
          "variable" : "gps_location",
          "location" : {
              "lat" : float(latitude),
              "lng" : float(longitude)
            }
        }
    ]
    response = requests.post(piURL, headers=piHeaders, data=json.dumps(gpsData))
    print(f"GPS updated: {latitude}, {longitude} | {response.text}")

def dash_scan():
    print("Checking for emergency...")
    total_sleep = 0
    while True:
        if scan_stop_event.is_set():
            return
        else:
            if total_sleep == 10: # Run command every 10s
                 apiCommand = "python3 QLD_Fire_API.py"
                 frequency, seconds, saveMP3, emergency = get_SDR_params() # Check emergency state on dash
                 if int(emergency) != 0:
                     print(f"Recording params: freq - {frequency}MHz, duration - {seconds}s, savingToMP3Player - {saveMP3}")
                     record_command = f"python3 Record_Audio.py {frequency} {seconds} {saveMP3}"
                     subprocess.run(record_command, shell=True)
                     emergencyComplete = [
                         {
                           "variable" : "emergency",
                           "value" : 0
                         }
                     ]
                     response = requests.post(piURL, headers=piHeaders, data=json.dumps(emergencyComplete))
                     print("Emergency recording complete")
                 
                 total_sleep = 0 # Reset timer
        
            time.sleep(2)
            total_sleep += 2


def emergency_apis():
    print("APIs running...")
    total_sleep = 0
    while True:
        if scan_stop_event.is_set():
            return
        else:
            if total_sleep == 300: # Run command every 5 mins
                 apiCommand = "python3 QLD_Fire_API.py"
                 subprocess.run(apiCommand, shell=True, check=True)
                 total_sleep = 0 # Reset timer
        
            time.sleep(2)
            total_sleep += 2


ble_id = 0x1295
device_queue = asyncio.Queue()

def convert_to_coords(payload):
    latInt = payload[6:8]
    latDec = payload[8:14]
    latitude = f"-{latInt}.{latDec}"
    longInt = payload[16:19]
    longDec = payload[19:25]
    longitude = f"{longInt}.{longDec}"
    return latitude, longitude

def detection_callback(device, advertisement_data):
    for company_id, data in advertisement_data.manufacturer_data.items():
        if company_id == ble_id:
            asyncio.create_task(device_queue.put(data.hex()))

def run_ble_async(ble_detected_event):
    async def inner():
        scanner = BleakScanner()
        scanner.register_detection_callback(detection_callback)

        async with scanner:
            try:
                result = await asyncio.wait_for(device_queue.get(), timeout=60)  # Optional timeout
                latitude, longitude = convert_to_coords(result)
                upload_GPS(latitude, longitude)  # Push to tago
                ble_detected_event.set()  # Signal that BLE was found
                # Drain remaining queue items (prevents multiple triggers from same event)
                while not device_queue.empty():
                    try:
                        device_queue.get_nowait()
                    except queue.Empty:
                        break
            except asyncio.TimeoutError:
                #print("BLE scan timed out.")
                time.sleep(1)

    asyncio.run(inner())

def ble_scan():
    print("Scanning BLE...")
    #eventDetected = False
    total_sleep = 0
    cooldown_seconds = 60  # Cooldown period to avoid multiple recordings
    last_trigger_time = 0
    ble_detected_event = threading.Event()

    # Start BLE async scanner in a new thread
    ble_thread = threading.Thread(target=run_ble_async, args=(ble_detected_event,))
    ble_thread.start()
    
    
    while total_sleep < 3000:
        if scan_stop_event.is_set():
            return
        elif ble_detected_event.is_set():
            current_time = time.time()
            if current_time - last_trigger_time >= cooldown_seconds:
                frequency, seconds, saveMP3, emergency = get_SDR_params() # Get SDR recording params from dashboard
                print(f"Recording params: freq - {frequency}MHz, duration - {seconds}s, savingToMP3Player - {saveMP3}")
                record_command = f"python3 Record_Audio.py {frequency} {seconds} {saveMP3}"
                subprocess.run(record_command, shell=True)
            
            ble_detected_event.clear()
        
        time.sleep(2)
        total_sleep += 2
            

def signal_handler(sig, frame):
    scan_stop_event.set()
    print("\nScanning ended")

signal.signal(signal.SIGINT, signal_handler)


def main():
    ble_tid = threading.Thread(target=ble_scan, daemon=True)
    ble_tid.start() # Start BLE scanning
    
    api_tid = threading.Thread(target=emergency_apis, daemon=True)
    api_tid.start() # Start QLD emergency database apis

    dash_tid = threading.Thread(target=dash_scan, daemon=True)
    dash_tid.start() # Start dashboard HTTP scanning (event detection) 
    
    try:
        while ble_tid.is_alive():
            ble_tid.join(timeout=1)
    except KeyboardInterrupt:
        # This will now be handled by signal_handler
        pass


if __name__ == "__main__":
    main()
