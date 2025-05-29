import requests
import json
import sys
import os

# Dashboard details - reference: https://api.docs.tago.io/#85343179-19bb-455c-8f45-6705948b3966
tagoIOurl = 'https://api.us-e1.tago.io/files'
headers = {
    'Authorization' : 'API KEY'
}


def upload_mp3_to_tago(f, filePath):
    # Step 1: Open file and alert Tago of upload
    t = True
    filename = os.path.basename(filePath)

    payload_1 = {
        "multipart_action": "start", 
        "filename": filename,
        "public": str(t).lower(), 
        "contentType": "audio/mp3"
    }

    response_1 = requests.request("POST", tagoIOurl, headers=headers, data=payload_1)
    response_1 = response_1.json()

    if response_1["status"]:
        upload_id = response_1["result"]

        # Step 2: Upload part
        with open(filePath, 'rb') as fobj:
            files = {
                "file": (filename, fobj, "audio/mp3")
            }

            data = {
                "multipart_action": "upload",
                "filename": filename,
                "upload_id": upload_id,
                "part": 1
            }

            response_2 = requests.post(tagoIOurl, headers=headers, data=data, files=files)
            response_2 = response_2.json()

            # Step 3: Confirm upload has completed
            if response_2["status"]:
                etag = response_2["result"]["ETag"]
                payload_3 = {
                    "multipart_action": "end",
                    "upload_id": upload_id,
                    "filename": filename,
                    "parts": [
                        {
                            "ETag": etag,
                            "PartNumber": 1
                        }
                    ]
                }
                response_3 = requests.request("POST", tagoIOurl, headers=headers, json=payload_3)
                response_3 = response_3.json()
                tagoFile = response_3["result"]["file"]

                # Final Step: Send tagoIO file URL to dashboard variable
                piURL = 'https://api.us-e1.tago.io/data'
                piHeaders = {
                    'device-token': '7c8e65cf-e89a-4350-bf2c-82d76db291dd',
                    'Content-Type': 'application/json'
                }
                piFileData = [
                    {
                      "variable": "audioURL",
                      "value": tagoFile
                    }
                ]
                response = requests.post(piURL, headers=piHeaders, data=json.dumps(piFileData))
                print(response)
            else:
                print("error with upload")

    else:
        print("error with tago")


def main():
    if len(sys.argv) == 3:
        f = sys.argv[1]
        filePath = sys.argv[2]
        upload_mp3_to_tago(f, filePath)
        print("SUCCESS")
    else:
        print("Usage error: (.\Audio_Dashboard frequency fileName)")


if __name__ == "__main__":
    main()

