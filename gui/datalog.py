import influxdb_client, os, time
from influxdb_client import InfluxDBClient, Point, WritePrecision
from influxdb_client.client.write_api import SYNCHRONOUS
import time

INFLUXDB_TOKEN="wJmCnn5sL-6yBFoiHlhQcylFfxtwkLMyc6qPXUYTF4gBe95MFlNZF7qWiAqHMFrOCSdpvDo5XCCIPDDwIO3M3Q=="
org = "CSSE4011 P3B"
url = "https://us-east-1-1.aws.cloud2.influxdata.com"
client = InfluxDBClient(url=url, token=INFLUXDB_TOKEN, org=org)
bucket="test4011"

write_api = client.write_api(write_options=SYNCHRONOUS)



def influxSend(queueLogs):
    while True:
        dataObj = queueLogs.get()

        X = -dataObj["x"]
        Y = dataObj["y"]
        V = dataObj["velocity"]
        numObjs = dataObj["numObj"]
        frameNum = dataObj["frNum"]

        for i in range(0, numObjs):
            point = (
                Point("mmWave_Test")
                .field("Frame_Number", frameNum)
                .field("Object_Number", i)
                .field("V", V[i])
                .field("x", X[i])
                .field("y", Y[i])
                )
            
            write_api.write(bucket=bucket, org=org, record=point)
        
