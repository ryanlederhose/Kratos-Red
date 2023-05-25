import tkinter as tk
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure
import numpy as np
from tkinter import messagebox
import serial
import serial.tools.list_ports
import threading, queue, pyautogui
from mmWaveProcessing import *
import math
import influxdb_client, os, time
from influxdb_client import InfluxDBClient, Point, WritePrecision
from influxdb_client.client.write_api import SYNCHRONOUS

INFLUXDB_TOKEN="wJmCnn5sL-6yBFoiHlhQcylFfxtwkLMyc6qPXUYTF4gBe95MFlNZF7qWiAqHMFrOCSdpvDo5XCCIPDDwIO3M3Q=="
org = "CSSE4011 P3B"
url = "https://us-east-1-1.aws.cloud2.influxdata.com"
client = InfluxDBClient(url=url, token=INFLUXDB_TOKEN, org=org)
bucket="test4011"

write_api = client.write_api(write_options=SYNCHRONOUS)

history = []
current_button = None
FLAG = 0

Z_BOUND = 0.2
X_BOUND = 0.2

queueXY = queue.Queue(maxsize=5)
queueLogs = queue.Queue(maxsize=1)

button_queue = queue.Queue()

def get_flag():
    global FLAG
    return FLAG

def on_closing():
    global FLAG
    print("Closing GUI and stopping all Threads")
    FLAG = -1
    root.quit()

def bsu(ser):

    button_queue.queue.clear()
    ser.write(b"scan f\n")
    print(ser.readline().decode('utf-8'))
    print(ser.readline().decode('utf-8'))
    print(ser.readline().decode('utf-8'))
    ser.write(b"scan o\n")
    print(ser.readline().decode('utf-8'))
    print(ser.readline().decode('utf-8'))
    print(ser.readline().decode('utf-8'))
    while True:

        if get_flag() == -1:
            print("Stopping BSU thread")
            break

        button_idx = -1
        
        # Dequeue a button index
        if button_queue.empty() == False:
            button_idx = button_queue.get(block=False)
            print(f"Button {button_idx} pressed")
        
        # Process the button index as needed
        # LEFT button
        if button_idx == 0:
            ser.write(b"gesture 8\n")

        # UP button
        elif button_idx == 1:
            ser.write(b"gesture 5\n")

        # DOWN button
        elif button_idx == 2:
            ser.write(b"gesture 6\n")

        # RIGHT button
        elif button_idx == 3:
            ser.write(b"gesture 7\n")

        # DIAG UP
        elif button_idx == 4:
            ser.write(b"gesture 3\n")

        # CW button
        elif button_idx == 5:
            ser.write(b"gesture 1\n")

        # CCW button
        elif button_idx == 6:
            ser.write(b"gesture 2\n")

        # DIAG DOWN 
        elif button_idx == 7:
            ser.write(b"gesture 4\n")

        # RESET button
        elif button_idx == 8:
            ser.write(b"reset\n")

        if (button_idx != -1):
            print(ser.readline().decode('utf-8'))
            print(ser.readline().decode('utf-8'))
            print(ser.readline().decode('utf-8'))

        time.sleep(0.5)

def handle_hid(direction):
    distance = 500
    if mouse_enable.get() == 1:
        try:
            if direction == 1: #UP
                pyautogui.move(0, -distance)
            elif direction == 2: #DOWN
                pyautogui.move(0, distance)
            elif direction == 0: #LEFT
                pyautogui.move(-distance, 0)
            elif direction == 3: #RIGHT
                pyautogui.move(distance, 0)
            elif direction == 4: #FLIP UP
                pyautogui.click(button='right')
            elif direction == 7: #FLIP DOWN
                pyautogui.click(button='left')
            elif direction == 5: #CW
                center_x, center_y = pyautogui.position()
                radius = 50
                steps = 5
                for step in range(steps):
                    angle = (2 * math.pi * step) / steps
                    x = center_x + radius * math.cos(angle)
                    y = center_y + radius * math.sin(angle)
                    pyautogui.moveTo(x, y)
            elif direction == 6: #CCW
                center_x, center_y = pyautogui.position()
                radius = 50
                steps = 5
                for step in range(steps):
                    angle = (2 * math.pi * step) / steps
                    x = center_x - radius * math.cos(angle)
                    y = center_y + radius * math.sin(angle)
                    pyautogui.moveTo(x, y)
            else:
                return

        except pyautogui.FailSafeException:
            return


def button_clicked(idx):
    global current_button
    global history

    if current_button is not None:
        buttons[current_button].config(bg='white')
    
    buttons[idx].config(bg='red')
    current_button = idx
    history.append(buttons[idx]['text'])
    
    if len(history) > 4:
        history = history[-4:]
    
    update_history_text()

    handle_hid(idx)

    # Enqueue the button index
    button_queue.put(idx)
    
def update_history_text():
    history_text.delete(1.0, tk.END)
    for item in history:
        history_text.insert(1.0, item + "\n")

def plot_scatter():
    global ax1, canvas, ax2
    while True:
        
        if get_flag() == -1:
            print("Stopping Plot Thread")
            break

        detObj = queueXY.get()

        # print(detObj)
        # Generate random points for the scatter plot
        x = detObj["x"]
        y = detObj["y"]
        v = detObj["v"]
        # print("x:", x)
        # print("y:", y)
        # print("V:", v)

        # 2D plot update
        ax1.cla()
        ax1.set(xlim=(-0.5, 0.5), ylim=(0, 1.5))
        ax1.scatter(x, y, c='blue')
        
        ax1.set_xlabel('X position (m)')
        ax1.set_ylabel('Y position (m)')
        ax1.set_title('2D mmWave Scatter Plot')
      
        # 3D Plot Update
        ax2.cla()
        ax2.set(xlim=(-0.5, 0.5), ylim=(0, 1.5), zlim=(-1, 1))
        ax2.scatter(x, y, v, c='blue')
        
        ax2.set_xlabel('X position (m)')
        ax2.set_ylabel('Y position (m)')
        ax2.set_zlabel('Z position (m)')
        ax2.set_title('3D mmWave Plot')
        
        # Redraw the Scatter Plots
        canvas.draw()
        time.sleep(0.05)


x = np.zeros(80)
y = np.zeros(80)
v = np.zeros(80)

OFFSET = 20

frNum = 0

def influxSend():
    global queueLogs
    prevTime = 0
    current_time = 0

    while True:

        if not queueLogs.empty():
            current_time = time.time()

        if current_time > prevTime + 1:
            prevTime = time.time()
            dataObj = queueLogs.get()

            X = -dataObj["x"].item()
            Y = dataObj["y"].item()
            V = dataObj["v"]
            Z = dataObj["z"].item()
            ObjNum = dataObj["objNum"]
            frameNum = dataObj["frNum"]

            
            point = (
                Point("mmWave_Test")
                .field("Frame_Number", frameNum)
                .field("Object_Number", ObjNum)
                .field("V", V)
                .field("x", X)
                .field("y", Y)
                .field("z", Z)
                )
                
            write_api.write(bucket=bucket, org=org, record=point)
        
        time.sleep(0.1)




def mmw(cliPort, dataPort):
    global frNum
    global prevMove
    # device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    # repoPath = os.path.expanduser("~/Kratos-Red/mmwave_cfgs/")

    configFileName = "radar_config2.cfg"

    prevMove = ""

    serialConfig(cliPort, configFileName)

    cliPort.close()
    t = 0

    totalBytesParsed = 0
    line =  dataPort.readline()
    data = line

    firstCentreForDouble = False

    while True:
        line = dataPort.readline()
        data += line
        # line = dataPort.read(dataPort.in_waiting)
        # data += line

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
        
        if (parser_result == 0):
            totalBytesParsed += (headerStartIndex+totalPacketNumBytes) 

            detObj = {"numObj": numDetObj, "x": detectedX_array, "y": detectedY_array, "z": detectedZ_array, "v":detectedV_array}

            frNum += 1

            if not ("numObj" in detObj.keys()): # This probably never happens
                continue

            closestPoint = torch.Tensor([100,100,100])
            closestY = closestPoint[1]
            closestV = -1
            objNumTmp = 0
            objNum = 0
            
            for x, y, z, v in zip(detectedX_array, detectedY_array, detectedZ_array, detectedV_array):
                objNumTmp += 1
                if (v != 0):
                    point = torch.Tensor([x,y,z])

                    if (y < closestY):
                            closestPoint = torch.Tensor([x, y, z])
                            closestNorm = closestPoint[1]
                            closestV = v
                            objNum = objNumTmp
                            

            if closestY == closestPoint[1]:
                continue

            # print("---------------------------")
            # print("Object detected at ")
            # print("(x, y, z): ", str("(") + str(closestPoint[0]), str(closestPoint[1]) + str(")"))

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

            # print(posString)
            # print(prevMove)
            # print("---------------------------")

            if (posString == "Bottom centre" and prevMove == "Middle centre"):
                print("Going down")
                button_clicked(2)
            if (posString == "Top centre" and prevMove == "Middle centre"):
                print("Going up")
                button_clicked(1)
            if (posString == "Middle left" and prevMove == "Middle centre"):
                print("Going left")
                button_clicked(0)
            if (posString == "Middle right" and prevMove == "Middle centre"):
                print("Going right")
                button_clicked(3)
            if ((posString == "Middle right" or posString == "Bottom right") and prevMove == "Top right"):
                print("Right Click")
                button_clicked(4)
            if ((posString == "Middle left" or posString == "Bottom left") and prevMove == "Top left"):
                print("Left Click")
                button_clicked(7)
            if (posString == "Middle centre" and prevMove == "Middle centre" and firstCentreForDouble):
                button_clicked(5)
                print("DO A BARREL ROLL")

            prevMove = posString

            if (posString == "Middle centre"):
                firstCentreForDouble = not firstCentreForDouble
                print("Centered")
            else:
                firstCentreForDouble = False
           
            if not queueXY.full(): 
                queueXY.put(detObj)

            if not queueLogs.full(): 
                detObj = {"frNum" : frNum, 
                          "x" : closestPoint[0], 
                          "y" : closestPoint[1], 
                          "z" : closestPoint[2],
                          "v" : closestV,
                          "objNum" : objNum}
                
                queueLogs.put(detObj)
"""
            if (posString == "Bottom right"):
                button_clicked(1-1)

            elif (posString == "Bottom left"):
                button_clicked(2-1)
            
            elif (posString == "Top left"):
                button_clicked(3-1)

            elif (posString == "Top right"):
                button_clicked(4-1)

            elif (posString == "Top centre"):
                button_clicked(5-1)

            elif (posString == "Bottom centre"):
                button_clicked(6-1)

            elif (posString == "Middle right"):
                button_clicked(7-1)

            elif (posString == "Middle left"):
                button_clicked(8-1)

            # elif (posString == "Middle centre"):
            #     button_clicked(8)
"""



    
BSU_connected = "Failed to connect to BSU"
mmW_cli_connected = "Failed to connect to mmW CLI"
mmW_data_connected = "Failed to connect to mmW data"

def connect_to_com4():
    # Connection to BSU and mmW
    global BSU_connected, mmW_cli_connected, mmW_data_connected

    donglePort = "COM9"
    cliPort = "COM7"
    dataPort = "COM6"

    ports = serial.tools.list_ports.comports()

    try:
        dongle = serial.Serial(donglePort, baudrate=115200, timeout=1)
        dongle.write(b"\n")


        print("Starting BSU Thread")
        bsu_thread = threading.Thread(target=bsu, args=(dongle,))
        bsu_thread.start()
        BSU_connected = "Connected to BSU on " + donglePort

    
    except serial.serialutil.SerialException:
        print("Could not connect to " + donglePort)

    try:
        cli = serial.Serial(cliPort, baudrate=115200)
        mmW_cli_connected = "Connected to mmW CLI on " + cliPort
    except serial.serialutil.SerialException:
        print("Could not connect to " + cliPort) 
        return

    try:
        data = serial.Serial(dataPort, baudrate=921600) 
        mmW_data_connected = "Connected to mmW Data on " + dataPort        

    except serial.serialutil.SerialException:
        print("Could not connect to " + dataPort) 
        return
    
    print("Starting mmW Thread")
    mmw_thread = threading.Thread(target=mmw, args=(cli, data))
    mmw_thread.start()

    scatter_thread = threading.Thread(target=plot_scatter)
    scatter_thread.start()
    database_thread = threading.Thread(target=influxSend)
    database_thread.start()

    messagebox.showinfo("Connection Status", BSU_connected + "\n" + 
            mmW_cli_connected + "\n" + mmW_data_connected)




root = tk.Tk()
root.title("Button History GUI")

plot_frame = tk.Frame(root)
plot_frame.pack(side=tk.LEFT, padx=10, pady=10)

bottom_frame = tk.Frame(root)
bottom_frame.pack(side=tk.TOP, pady=10)

button_frame = tk.Frame(root)
button_frame.pack(padx=10, pady=10)

checkbox_frame = tk.Frame(root)
checkbox_frame.pack(padx=10, pady=10)

history_frame = tk.Frame(root)
history_frame.pack(side=tk.BOTTOM, padx=10, pady=10)

buttons = []

symbols = ['←', '↑', '↓', '→', '➚', '↻', '↺', '➘']

for i in range(4):
    button = tk.Button(button_frame, text=symbols[i], font=('Arial', 16), command=lambda idx=i: button_clicked(idx))
    button.grid(row=0, column=i, padx=5, pady=5)
    buttons.append(button)

for i in range(4, 8):
    button = tk.Button(button_frame, text=symbols[i], font=('Arial', 16), command=lambda idx=i: button_clicked(idx))
    button.grid(row=1, column=i-4, padx=5, pady=5)
    buttons.append(button)


history_label = tk.Label(history_frame, text="History:")
history_label.pack()

history_text = tk.Text(history_frame, height=4, width=10, font=('Arial', 16))
history_text.pack(side=tk.BOTTOM)

# Create the scatter plots
fig = Figure(figsize=(10, 5), dpi=100)
ax1 = fig.add_subplot(121)
ax2 = fig.add_subplot(122, projection='3d')

ax1.set_xlabel('X position (m)')
ax1.set_ylabel('Y position (m)')
ax1.set_title('2D mmWave Scatter Plot')
ax1.set(xlim=(-0.5, 0.5), ylim=(0, 1.5))

ax2.set_xlabel('X position (m)')
ax2.set_ylabel('Y position (m)')
ax2.set_zlabel('Z position (m)')
ax2.set_title('3D mmWave Plot')

fig.tight_layout(pad=2.0)
fig.subplots_adjust(wspace=0.1)

# plot_scatter(ax1)
# plot_scatter(ax2)/

# Add the LED RESET and CONNECT buttons        
reset_button = tk.Button(bottom_frame, text="RESET LED", font=('Arial', 16), command=lambda:button_queue.put(8  ))
reset_button.pack(side=tk.TOP, padx=1, pady=10)

connect_button = tk.Button(bottom_frame, text="CONNECT", font=('Arial', 16), command=connect_to_com4)
connect_button.pack(side=tk.TOP, padx=1, pady=10)

flag_state = tk.BooleanVar()
checkbox = tk.Checkbutton(checkbox_frame, text="Collect Data", variable=flag_state)
checkbox.pack(side=tk.BOTTOM, padx=1, pady=10)

mouse_enable = tk.BooleanVar()
checkbox = tk.Checkbutton(checkbox_frame, text="Mouse Toggle", variable=mouse_enable)
checkbox.pack(side=tk.BOTTOM, padx=1, pady=10)

# Add the plots to the GUI
canvas = FigureCanvasTkAgg(fig, master=plot_frame)
canvas.draw()
canvas.get_tk_widget().pack(side=tk.TOP, fill=tk.BOTH, expand=1)

# Protocol When Closing the GUI 
root.protocol("WM_DELETE_WINDOW", on_closing)

root.mainloop()
