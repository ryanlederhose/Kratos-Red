import serial
import numpy as np
import torch
import time
import os
import queue
import serial
from colorama import Fore
from parser_mmw_demo import parser_one_mmw_demo_output_packet

byteBuffer = np.zeros(2**15, dtype='uint8')
byteBufferLength = 0

Z_BOUND = 0.3
X_BOUND = 0.3

# ------------------------------------------------------------------

queueXY = queue.Queue(maxsize=5)
queueLogs = queue.Queue(maxsize=1)


PROGRAM_PORT = '/dev/ttyACM0'

def serialConfig(progPort, configFile: str):
    """
    This uploads the config file to the radar module on PROGRAM_PORT. Takes the file location as input for where to upload file 
    """

    f = open(configFile, "r")
    for x in f:
        if x[0] == '%':
            continue
        try:
            progPort.write(x.encode())
            printSend(x.replace('\n',''))
            line =  progPort.readline().decode().replace('\r','').replace('\n','')
            printRecieve(str(line))
            while (line != 'mmwDemo:/>'):
                if (x == 'sensorStart'):
                    continue
                line =  progPort.readline().decode().replace('\r','').replace('\n','')
                printRecieve(str(line))
        except UnicodeDecodeError:
            print("Bad read, trying again")
            progPort.write(x.encode())
            printSend(x.replace('\n',''))
            line =  progPort.readline().decode().replace('\r','').replace('\n','')
            printRecieve(str(line))
            while(line != 'mmwDemo:/>'):
                line =  progPort.readline().decode().replace('\r','').replace('\n','')
                printRecieve(str(line))



def printSend(line: str):
    """
    Prints input to in GREEN text, speciffically used for printting messages sent over COM port
    """
    print(Fore.GREEN + line + Fore.WHITE)


def printRecieve(line: str):
    """
    Prints input to in BLUE text, speciffically used for printting messages recieved over COM port
    """
    print(Fore.BLUE + line + Fore.WHITE)



# ------------------------------------------------------------------

# Function to parse the data inside the configuration file
def parseConfigFile(configFileName):
    configParameters = {} # Initialize an empty dictionary to store the configuration parameters
    
    # Read the configuration file and send it to the board
    config = [line.rstrip('\r\n') for line in open(configFileName)]
    for i in config:
        
        # Split the line
        splitWords = i.split(" ")
        
        # Hard code the number of antennas, change if other configuration is used
        numRxAnt = 4
        numTxAnt = 3
        
        # Get the information about the profile configuration
        if "profileCfg" in splitWords[0]:
            startFreq = int(float(splitWords[2]))
            idleTime = int(splitWords[3])
            rampEndTime = float(splitWords[5])
            freqSlopeConst = float(splitWords[8])
            numAdcSamples = int(splitWords[10])
            numAdcSamplesRoundTo2 = 1;
            
            while numAdcSamples > numAdcSamplesRoundTo2:
                numAdcSamplesRoundTo2 = numAdcSamplesRoundTo2 * 2;
                
            digOutSampleRate = int(splitWords[11]);
            
        # Get the information about the frame configuration    
        elif "frameCfg" in splitWords[0]:
            
            chirpStartIdx = int(splitWords[1]);
            chirpEndIdx = int(splitWords[2]);
            numLoops = int(splitWords[3]);
            numFrames = int(splitWords[4]);
            framePeriodicity = float(splitWords[5]);

            
    # Combine the read data to obtain the configuration parameters           
    numChirpsPerFrame = (chirpEndIdx - chirpStartIdx + 1) * numLoops
    configParameters["numDopplerBins"] = numChirpsPerFrame / numTxAnt
    configParameters["numRangeBins"] = numAdcSamplesRoundTo2
    configParameters["rangeResolutionMeters"] = (3e8 * digOutSampleRate * 1e3) / (2 * freqSlopeConst * 1e12 * numAdcSamples)
    configParameters["rangeIdxToMeters"] = (3e8 * digOutSampleRate * 1e3) / (2 * freqSlopeConst * 1e12 * configParameters["numRangeBins"])
    configParameters["dopplerResolutionMps"] = 3e8 / (2 * startFreq * 1e9 * (idleTime + rampEndTime) * 1e-6 * configParameters["numDopplerBins"] * numTxAnt)
    configParameters["maxRange"] = (300 * 0.9 * digOutSampleRate)/(2 * freqSlopeConst * 1e3)
    configParameters["maxVelocity"] = 3e8 / (4 * startFreq * 1e9 * (idleTime + rampEndTime) * 1e-6 * numTxAnt)
    
    return configParameters
# ------------------------------------------------------------------


x = np.zeros(80)
y = np.zeros(80)
v = np.zeros(80)

OFFSET = 20

frNum = 0


def mmw(cliPort, dataPort):
    global frNum
    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    repoPath = os.path.expanduser("~/Kratos-Red/mmwave_cfgs/")

    configFileName = repoPath + "radar_config2.cfg"
    
    MAX_NUM_OBJS = 10
    NUM_FRAMES=80
    frameNumber = 0
    
    numMovingPoints = 0
    numPoints = 0

    startSeq = False

    serialConfig(cliPort, configFileName)

    configParameters = parseConfigFile(configFileName)

    t = 0

    totalBytesParsed = 0
    line =  dataPort.readline()
    data = line

    while True:
        line = dataPort.readline()
        data += line
        
        parser_result, \
        headerStartIndex,  \
        totalPacketNumBytes, \
        numDetObj,  \
        numTlv,  \
        subFrameNumber,  \
        detectedX_array,  \
        detectedY_array,  \
        detectedZ_array,  \
        detectedV_array,  \
        detectedRange_array,  \
        detectedAzimuth_array,  \
        detectedElevation_array,  \
        detectedSNR_array,  \
        detectedNoise_array = parser_one_mmw_demo_output_packet(data[totalBytesParsed::1], len(data)-totalBytesParsed, enablePrint=False, errorPrint=False) 

        t += 1
        timeLen = 0



        if (parser_result == 0):
            totalBytesParsed += (headerStartIndex+totalPacketNumBytes) 

            detObj = {"numObj": numDetObj, "x": detectedX_array, "y": detectedY_array, "z": detectedZ_array, "v":detectedV_array}

            frNum += 1

            if not ("numObj" in detObj.keys()): # This probably never happens
                continue

            if not queueXY.full(): 
                queueXY.put(detObj)

            if not queueLogs.full(): 
                detObj["frNum"] = frNum
                queueLogs.put(detObj)

            closestPoint = torch.Tensor([100,100,100])
            closestNorm = closestPoint.norm()
            
            for x, y, z, v in zip(detectedX_array, detectedY_array, detectedZ_array, detectedV_array):
                if (v != 0):
                    point = torch.Tensor([x,y,z])
                    norm = point.norm()

                    if (norm < closestNorm):
                            closestPoint = torch.Tensor([x, y, z])
                            closestNorm = closestPoint.norm()

            if closestNorm == torch.Tensor([100, 100, 100]).norm():
                continue

            print("Object detected at ")
            print("(x, y, z): ", str("(") + str(closestPoint[0]), str(closestPoint[1]) + str(")"))

            posString = ""

            if closestPoint[2] < Z_BOUND:
                posString += "Bottom"

            elif closestPoint[2] < Z_BOUND * 2:
                posString += "Middle"
            
            else:
                posString += "Top"

            posString += " "

            if closestPoint[0] < -X_BOUND:
                posString += "right"

            elif closestPoint[0] > X_BOUND: 
                posString += "left"

            else:
                posString += "centre"


            print(posString)



           
