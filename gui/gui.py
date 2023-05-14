import tkinter as tk
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure
import numpy as np
from tkinter import messagebox
import serial
import serial.tools.list_ports


class ButtonHistoryGUI:
    def __init__(self):
        self.history = []
        self.current_button = None
        self.root = tk.Tk()
        self.root.title("Button History GUI")
        
        self.plot_frame = tk.Frame(self.root)
        self.plot_frame.pack(side=tk.LEFT, padx=10, pady=10)

        bottom_frame = tk.Frame(self.root)
        bottom_frame.pack(side=tk.TOP, pady=10)
        
        self.button_frame = tk.Frame(self.root)
        self.button_frame.pack(padx=10, pady=10)
        
        self.history_frame = tk.Frame(self.root)
        self.history_frame.pack(side=tk.BOTTOM, padx=10, pady=10)
        
        self.buttons = []
        
        symbols = ['←', '↑', '↓', '→','↻', '↺']
        
        for i in range(4):
            button = tk.Button(self.button_frame, text=symbols[i], font=('Arial', 16), command=lambda idx=i: self.button_clicked(idx))
            button.grid(row=0, column=i, padx=5, pady=5)
            self.buttons.append(button)
        
        for i in range(4, 6):
            button = tk.Button(self.button_frame, text=symbols[i], font=('Arial', 16), command=lambda idx=i: self.button_clicked(idx))
            button.grid(row=1, column=i-3, padx=5, pady=5)
            self.buttons.append(button)
        
        self.history_label = tk.Label(self.history_frame, text="History:")
        self.history_label.pack()
        
        self.history_text = tk.Text(self.history_frame, height=4, width=10, font=('Arial', 16))
        self.history_text.pack()
        
        # Create the scatter plots
        fig = Figure(figsize=(5, 5), dpi=100)
        ax1 = fig.add_subplot(211)
        ax2 = fig.add_subplot(212)
        self.plot_scatter(ax1)
        self.plot_scatter(ax2)

        # Add the LED RESET and CONNECT buttons        
        reset_button = tk.Button(bottom_frame, text="RESET LED", font=('Arial', 16))
        reset_button.pack(side=tk.TOP, padx=1, pady=10)
        
        connect_button = tk.Button(bottom_frame, text="CONNECT", font=('Arial', 16), command=self.connect_to_com4)
        connect_button.pack(side=tk.TOP, padx=1, pady=10)
        
        # Add the plots to the GUI
        canvas = FigureCanvasTkAgg(fig, master=self.plot_frame)
        canvas.draw()
        canvas.get_tk_widget().pack(side=tk.TOP, fill=tk.BOTH, expand=1)


        
    def button_clicked(self, idx):
        if self.current_button is not None:
            self.buttons[self.current_button].config(bg='white')
        
        self.buttons[idx].config(bg='red')
        self.current_button = idx
        self.history.append(self.buttons[idx]['text'])
        
        if len(self.history) > 4:
            self.history = self.history[-4:]
        
        self.update_history_text()
        
    def update_history_text(self):
        self.history_text.delete(1.0, tk.END)
        for item in self.history:
            self.history_text.insert(tk.END, item + "\n")
    
    def plot_scatter(self, ax):
        # Generate random points for the scatter plot
        x = np.random.rand(50)
        y = np.random.rand(50)
        
        ax.scatter(x, y, c='blue')
        ax.set_xlabel('X')
        ax.set_ylabel('Y')
        ax.set_title('Scatter Plot')
        
    def run(self):
        self.root.mainloop()

    def connect_to_com4(self):
        # Simulating connection to COM4
        # Replace this with your actual connection logic
        connected = True  # Set this based on the connection status

        ports = serial.tools.list_ports.comports()
        for port in ports:
            print(f"Port: {port.device}, Description: {port.description}")
            try:
                ser = serial.Serial(port.device, baudrate=115200, timeout=1)
                print("connected to" + port.device)
                ser.write(b'\n')
                print(ser.readline().decode('utf-8'))

            except:
                print("could not connect to" + port.device)

        
        if connected:
            messagebox.showinfo("Connection Status", "Connected to COM4")
        else:
            messagebox.showinfo("Connection Status", "Failed to connect to COM4")

gui = ButtonHistoryGUI()
gui.run()
