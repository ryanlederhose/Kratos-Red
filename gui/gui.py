import tkinter as tk
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure
import numpy as np
from tkinter import messagebox
import serial
import serial.tools.list_ports
import threading, queue

history = []
current_button = None

button_queue = queue.Queue()

def main(ser):
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

    # Enqueue the button index
    button_queue.put(idx)
    
def update_history_text():
    history_text.delete(1.0, tk.END)
    for item in history:
        history_text.insert(1.0, item + "\n")

def plot_scatter(ax):
    # Generate random points for the scatter plot
    x = np.random.rand(50)
    y = np.random.rand(50)
    
    ax.scatter(x, y, c='blue')
    ax.set_xlabel('X')
    ax.set_ylabel('Y')
    ax.set_title('Scatter Plot')
    
BSU_connected = "Failed to connect to BSU"
mmW_connected = "Failed to connect to mmW"

def connect_to_com4():
    # Connection to BSU and mmW
    global BSU_connected, mmW_connected

    ports = serial.tools.list_ports.comports()
    for port in ports:
        print(f"Port: {port.device}, Description: {port.description}")
        if port.description == "USB Serial Device (" + port.device + ")":
            try:
                ser = serial.Serial(port.device, baudrate=115200, timeout=1)
                print("Connected to " + port.device)
            except serial.serialutil.SerialException:
                print("Could not connect to " + port.device)
                continue
            ser.write(b"\n")
            a = ser.readline().decode('utf-8')
            a = ser.readline().decode('utf-8')
            if a == '\x1b[1;32mBSU_SHELL:~$ \x1b[m':
                print("Starting BSU Thread")
                bsu_thread = threading.Thread(target=main, args=(ser,))
                bsu_thread.start()
                BSU_connected = "Connected to BSU on " + port.device
                
            else:
                ser.close()

    messagebox.showinfo("Connection Status", BSU_connected + "\n" + mmW_connected)




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
fig = Figure(figsize=(5, 5), dpi=100)
ax1 = fig.add_subplot(211)
ax2 = fig.add_subplot(212)
plot_scatter(ax1)
plot_scatter(ax2)

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

