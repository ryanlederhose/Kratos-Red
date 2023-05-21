import tkinter as tk
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure
import numpy as np
from tkinter import messagebox
import serial
import serial.tools.list_ports
import threading, queue, pyautogui
from mmWaveProcessing import *

history = []
current_button = None

button_queue = queue.Queue()

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

        # Dequeue a button index
        button_idx = button_queue.get()
        
        # Process the button index as needed
        # For example, print the index
        print(f"Button {button_idx} pressed")

        # LEFT button
        if button_idx == 0:
            ser.write(b"gesture 0\n")

        # UP button
        elif button_idx == 1:
            ser.write(b"gesture 1\n")

        # DOWN button
        elif button_idx == 2:
            ser.write(b"gesture 2\n")

        # RIGHT button
        elif button_idx == 3:
            ser.write(b"gesture 3\n")

        # CW button
        elif button_idx == 4:
            ser.write(b"gesture 4\n")

        # CCW button
        elif button_idx == 5:
            ser.write(b"gesture 5\n")

        # RESET button
        elif button_idx == 6:
            ser.write(b"reset\n")

        print(ser.readline().decode('utf-8'))
        print(ser.readline().decode('utf-8'))
        print(ser.readline().decode('utf-8'))

def handle_hid(direction):
    current_x, current_y = pyautogui.position()
    distance = 500

    if direction == 1: #UP
        new_x = current_x
        new_y = current_y - distance
    elif direction == 2: #DOWN
        new_x = current_x
        new_y = current_y + distance
    elif direction == 0: #LEFT
        new_x = current_x - distance
        new_y = current_y
    elif direction == 3: #RIGHT
        new_x = current_x + distance
        new_y = current_y
    else:
        return


    pyautogui.moveTo(new_x, new_y)

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
        detObj = queueXY.get()

        # print(detObj)
        # Generate random points for the scatter plot
        x = -detObj["x"]
        y = detObj["y"]
        v = detObj["velocity"]
        print("x:", x)
        print("y:", y)
        print("V:", velocity)

        # 2D plot update
        ax1.cla()
        ax1.set(xlim=(-0.5, 0.5), ylim=(0, 1.5))
        ax1.scatter(x, y, c='blue')
        
        # 3D Plot Update
        ax2.cla()
        ax2.set(xlim=(-0.5, 0.5), ylim=(0, 1.5), zlim=(-1, 1))
        ax2.scatter(x, y, v, c='blue')
        
        canvas.draw()
        # ax1.show()
    
BSU_connected = "Failed to connect to BSU"
mmW_cli_connected = "Failed to connect to mmW CLI"
mmW_data_connected = "Failed to connect to mmW data"

def connect_to_com4():
    # Connection to BSU and mmW
    global BSU_connected, mmW_cli_connected, mmW_data_connected

    donglePort = "/dev/ttyACM2"
    cliPort = "/dev/ttyACM0"

    ports = serial.tools.list_ports.comports()

    portNum = 0
    for port in ports:
        if port.device == cliPort:
            break
        portNum += 1

    try:
        dongle = serial.Serial(donglePort, baudrate=115200, timeout=1)
        dongle.write(b"\n")
        prompt1 = dongle.readline().decode('utf-8')
        prompt2 = dongle.readline().decode('utf-8')

        if prompt2 == '\x1b[1;32mnRF-Central:~$ \x1b[m':
            print("Starting BSU Thread")
            bsu_thread = threading.Thread(target=bsu, args=(dongle,))
            bsu_thread.start()
            BSU_connected = "Connected to BSU on " + donglePort
        else:
            dongle.close()
            raise serial.serialutil.SerialException
    
    except serial.serialutil.SerialException:
        print("Could not connect to " + donglePort)

    try:
        cli = serial.Serial(cliPort, baudrate=115200, timeout=1)
        cli.write(b"\n")
        prompt3 = cli.readline().decode('utf-8')
        prompt4 = cli.readline().decode('utf-8')

        if prompt4 == 'mmwDemo:/>':
            print("Starting mmW Thread")
            mmW_cli_connected = "Connected to mmW CLI on " + port.device
            data = serial.Serial(ports[portNum - 1].device, baudrate=921600, timeout=1) ##### CHANGE TO PORT_NUMBER - 1 for LINUX
            mmW_data_connected = "Connected to mmW Data on " + ports[portNum - 1].device
            mmw_thread = threading.Thread(target=mmw, args=(cli,data))
            mmw_thread.start()
            scatter_thread = threading.Thread(target=plot_scatter)
            scatter_thread.start()
        
        else:
            cli.close()
            raise serial.serialutil.SerialException

    except serial.serialutil.SerialException:
        print("Could not connect to " + cliPort)


    # ports = serial.tools.list_ports.comports()
    # port_number = 0
    # for port in ports:
    #     print(f"Port: {port.device}, Description: {port.description}")
    #     if port.description == "XDS110 (02.03.00.05) Embed with CMSIS-DAP":
    #         try:
    #             ser = serial.Serial(port.device, baudrate=115200, timeout=1)
    #             print("Connected to " + port.device)
    #         except serial.serialutil.SerialException:
    #             print("Could not connect to " + port.device)
    #             continue
    #         ser.write(b"\n")
    #         try:
    #             a = ser.readline().decode('utf-8')
    #             a = ser.readline().decode('utf-8')
    #             break
    #         except UnicodeDecodeError:
    #             ser.close()
    #         if a == '\x1b[1;32mBSU_SHELL:~$ \x1b[m':
    #             print("Starting BSU Thread")
    #             bsu_thread = threading.Thread(target=bsu, args=(ser,))
    #             bsu_thread.start()
    #             BSU_connected = "Connected to BSU on " + port.device
    #         elif a == 'mmwDemo:/>':
    #             print("Starting mmW Thread")
    #             mmW_cli_connected = "Connected to mmW CLI on " + port.device
    #             ser2 = serial.Serial(ports[port_number - 1].device, baudrate=921600, timeout=1) ##### CHANGE TO PORT_NUMBER - 1 for LINUX
    #             mmW_data_connected = "Connected to mmW Data on " + ports[port_number - 1].device
    #             mmw_thread = threading.Thread(target=mmw, args=(ser,ser2))
    #             mmw_thread.start()
    #             scatter_thread = threading.Thread(target=plot_scatter, args=(ax1))
    #             scatter_thread.start()
                
    #         else:
    #             ser.close()

    #         port_number += 1
        

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

symbols = ['←', '↑', '↓', '→','↻', '↺']

for i in range(4):
    button = tk.Button(button_frame, text=symbols[i], font=('Arial', 16), command=lambda idx=i: button_clicked(idx))
    button.grid(row=0, column=i, padx=5, pady=5)
    buttons.append(button)

for i in range(4, 6):
    button = tk.Button(button_frame, text=symbols[i], font=('Arial', 16), command=lambda idx=i: button_clicked(idx))
    button.grid(row=1, column=i-3, padx=5, pady=5)
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
reset_button = tk.Button(bottom_frame, text="RESET LED", font=('Arial', 16), command=lambda:button_queue.put(6))
reset_button.pack(side=tk.TOP, padx=1, pady=10)

connect_button = tk.Button(bottom_frame, text="CONNECT", font=('Arial', 16), command=connect_to_com4)
connect_button.pack(side=tk.TOP, padx=1, pady=10)

flag_state = tk.BooleanVar()
checkbox = tk.Checkbutton(checkbox_frame, text="Collect Data", variable=flag_state)
checkbox.pack(side=tk.BOTTOM, padx=1, pady=10)

# Add the plots to the GUI
canvas = FigureCanvasTkAgg(fig, master=plot_frame)
canvas.draw()
canvas.get_tk_widget().pack(side=tk.TOP, fill=tk.BOTH, expand=1)


root.mainloop()
