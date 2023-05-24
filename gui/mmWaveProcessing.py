import serial
import numpy as np
import torch
import time
import os
import queue
import serial
from colorama import Fore
from parser_mmw_demo import parser_one_mmw_demo_output_packet
# from gui import button_clicked



# ------------------------------------------------------------------



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
            # printSend(x)
            line =  progPort.readline().decode().replace('\r','').replace('\n','')
            printRecieve(str(line))

            validRetMsg = False

            while (line != 'mmwDemo:/>') and (line != 'sensorStop'):
                # if (line == 'Done'):
                #     break

                if ('mmwDemo:/>' in str(line)):
                    break
                # if (line != 'mmwDemo:/>'):
                #     validRetMsg = True

                # if ('sensorStop' in x):
                #     if ("Ignored: Sensor is already stopped" in line):
                #         validRetMsg = True

                if (x == 'sensorStart'):
                    progPort.write('\n'.encode())
                    line = progPort.readline().decode().replace('\r','').replace('\n','')
                    break
                progPort.write('\n'.encode())
                line =  progPort.readline().decode().replace('\r','').replace('\n','')
                printRecieve(str(line))

            time.sleep(0.1)

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




           
