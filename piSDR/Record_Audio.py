import subprocess
import sys
from datetime import datetime
import shutil
import os

# Constants for mp3 player - change if using different device
mount_point = "/mnt/mp3player"
device_path = "/dev/sda"

def is_mounted(mount):
    with open("/proc/mounts") as f:
        return any(mount in line for line in f)

def main():
    if len(sys.argv) == 4:    
        freq = sys.argv[1]
        length = sys.argv[2] 
        savetomp3 = sys.argv[3].strip().lower() == "true"
    else:
        print("Usage error: ./Record_Audio.py freq len mp3-(t/f)")
        return 1

    length = str(int(length) + 2) # Add 2 seconds to allow for setup
    recTime = datetime.now().strftime("%Y-%m-%d_%H.%M")
    fileName = f"./Recordings/Recording_{recTime}_{freq}MHz.mp3"
    command = (
        f"timeout {length} rtl_fm -f {freq}M -M wbfm -s 200k -r 48k -g 45 -l 50 - | "
        "sox -t raw -r 48k -e signed -b 16 -c 1 - output.wav && "
        f"lame output.wav {fileName}"
    )

    #print(command)
    print("Recording in progress...")

    result = subprocess.run(command, shell=True, capture_output=True, text=True)
    
    if result.returncode == 0: #Upload to dashboard
        command_2 = f"python3 Audio_Dashboard.py {freq} {fileName}"
        result_2 = subprocess.run(command_2, shell=True, capture_output=True, text=True)
        print("STDOUT:", result_2.stdout)
        #print("STDERR:", result_2.stderr)

        if result.returncode == 0 and savetomp3==True: # Transfer to mp3 player
            print("Saving to mp3 player")
            dest_path = f"/mnt/mp3player/{os.path.basename(fileName)}"
            if not is_mounted(mount_point): # Mount the mp3 if not already mounted
                try:
                    os.makedirs(mount_point, exist_ok=True)
                    subprocess.run(["sudo", "mount", device_path, mount_point], check=True)
                    print(f"Mounted {device_path} to {mount_point}")
                except subprocess.CalledProcessError as e:
                    print("Mount failed. Please check if the device path is correct and try manually.")
                    return 1
            try: # Copy the file across
                shutil.copyfile(fileName, dest_path)
                print(f"File copied to MP3 player: {dest_path}")
                try:
                    subprocess.run(["sudo", "umount", mount_point], check=True)
                    print(f"Unmounted {mount_point}")
                except subprocess.CalledProcessError:
                    print(f"Failed to unmount {mount_point}. You may need to unmount it manually.")

            except PermissionError:
                print("Permission denied: Try mounting the MP3 player with write access or use sudo.")
            except FileNotFoundError:
                print("MP3 player not mounted or path not found.")
            except Exception as e:
                print(f"Unexpected error during file transfer: {e}")

    else:
        print("Error during recording")
        print("STDOUT:", result.stdout)
        print("STDERR:", result.stderr)
        return 1

if __name__ == "__main__":
    main()
