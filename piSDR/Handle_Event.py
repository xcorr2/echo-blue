import requests
import json
import threading
import time
import signal
import subprocess

piURL = 'https://api.us-e1.tago.io/data'
piHeaderGET = {
    'device-token': '7c8e65cf-e89a-4350-bf2c-82d76db291dd'
}

scan_stop_event = threading.Event() # Create a stop event

def get_SDR_params():
    requestURL = "https://api.us-e1.tago.io/data?variables[]=frequency&variables[]=seconds&variables[]=saveMP3&query=last_item"
    responseGET = requests.request("GET", requestURL, headers=piHeaderGET)
    if responseGET.status_code == 200:
        responseGET = responseGET.json()
        frequency = responseGET["result"][0]["value"]
        seconds = responseGET["result"][1]["value"]
        saveMP3 = responseGET["result"][2]["value"]
        return frequency, seconds, saveMP3
    else:
        print("Error retrieving recording params from TagoIO")
        exit()

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

def ble_scan():
    eventDetected = False
    total_sleep = 0
    while total_sleep < 3000:
        if scan_stop_event.is_set():
            return
        else:
            #print("Scanning BLE...")
            #if eventDetected == True:
            if total_sleep == 16:
                 frequency, seconds, saveMP3 = get_SDR_params() # Get SDR recording params from dashboard
                 print(f"Recording params: freq - {frequency}MHz, duration - {seconds}s, savingToMP3Player - {saveMP3}")
                 record_command = f"python3 Record_Audio.py {frequency} {seconds} {saveMP3}"
                 subprocess.run(record_command, shell=True)
        
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
    
    try:
        while ble_tid.is_alive():
            ble_tid.join(timeout=1)
    except KeyboardInterrupt:
        # This will now be handled by signal_handler
        pass


if __name__ == "__main__":
    main()
