import requests
import json

qldFireURL = 'https://services1.arcgis.com/vkTwD8kHw2woKBqV/arcgis/rest/services/ESCAD_Current_Incidents_Public/FeatureServer/0/query'
qldTrafficURL = 'https://api.qldtraffic.qld.gov.au/v2/events'
traffic_api_key = '3e83add325cbb69ac4d8e5bf433d770b'


tagoIOurl = 'https://api.us-e1.tago.io/data'
headers = {
    'device-token': 'fa3edc08-5f30-48b0-9eb1-5fad91a22348',
    'Content-Type': 'application/json'
}

# Center point (longitude, latitude)
latitude = -27.468582300269144
longitude = 153.02385890504254  # Brisbane CBD as example

"""
params = {
    'where': '1=1',
    'outFields': '*',
    'returnGeometry': 'true',
    'f': 'geojson'
}
"""

"""
params = {
    'where': '1=1',
    'returnCountOnly': 'true',
    'f': 'json'
}
"""

def send_to_tago(jsonLoad, d_type):
    if d_type: # if d_type = 1, sending fire info
        data = [
            {
              "variable": "fires_and_rescue",
              "value": str(jsonLoad["count"])
            }
        ]
    else: # otherwise send road info
        data = jsonLoad
        
    response = requests.post(tagoIOurl, headers=headers, data=json.dumps(data))
    print(f"Status Code: {response.status_code}")
    print("Response Body:", response.text)



# These arguments determine the number of incidents within 100km
paramsFire = {
    'geometry': f"{longitude},{latitude}",
    'geometryType': 'esriGeometryPoint',
    'spatialRel': 'esriSpatialRelIntersects',
    'distance': 100000,
    'inSR': 4326,
    'where': '1=1',
    'returnCountOnly': 'true',
    'f': 'json'
}

paramsTMR = {
    'apikey': traffic_api_key,
    'status': 'Published'
}


responseFIRE = requests.get(qldFireURL, params=paramsFire)
responseTMR = requests.get(qldTrafficURL, params=paramsTMR)

if responseFIRE.status_code == 200:
    geojson_data = responseFIRE.json()
    send_to_tago(geojson_data, 1)
else:
    print("Request failed:", responseFIRE.status_code, responseFIRE.text)
    
if responseTMR.status_code == 200:
    geojson_data_TMR = responseTMR.json()
    features = geojson_data_TMR.get("features", [])

    num = 0
    road_events = []
    for f in features:
        p = f["properties"]
        event = p["event_type"]
        road = p["road_summary"]["road_name"]
        locality = p["road_summary"]["locality"]
        postcode = p["road_summary"]["postcode"]
        lga  = p["road_summary"]["local_government_area"]
        lga_l = lga.lower()
        
        if ("Hazard" in event or "Crash" in event or "Flooding" in event) and ("brisbane" in lga_l or "logan" in lga_l or "moreton" in lga_l or "ipswich" in lga_l):
            road_event = f"{p['event_subtype']} on {road} - {locality} {postcode}"
            #road_events.append(road_event)
            tago_payload = [{"variable": "road_incident", "value": road_event}]
            send_to_tago(tago_payload, 0)

    #num = 0
    tago_payload = [{"variable": "road_incident", "value": incident} for incident in road_events]
    send_to_tago(tago_payload, 0)
    
else:
    print("Request failed:", responseTMR.status_code, responseTMR.text)

