'''
This python script opens the serial port and reads incoming data from the
nRF peripheral dongle. It then outputs the appropriate image to the 
led matrix 
'''

# Imports
import serial
import sys
import math
import time
import copy

sys.path.append('/home/pi/rpi-rgb-led-matrix/bindings/python')
from rgbmatrix import RGBMatrix, RGBMatrixOptions, graphics

# Global Constants
DATALIST_EMPTY = ['', '', '', '', '', '', '', '']
COMMANDLIST_EMPTY = ['', '', '', '', '', '', '', '', '', '', '']
INPUT_EMPTY = ['', '']

COMMAND = 0
DATA = 1

RIGHT_SWIPE = 1
LEFT_SWIPE = 2
UP_SWIPE = 3
DOWN_SWIPE = 4
MOVE_RIGHT = 5
MOVE_LEFT = 6
MOVE_UP = 7
MOVE_DOWN = 8

GESTURE = 1
RESET = 2
CLEAR = 3
START_UP = 4

'''
LEDMatrix: Represents the AdaFruit 32 x 32 LED Matrix
'''
class LEDMatrix(object):

    '''
    LEDMatrix constructor
    '''
    def __init__(self, *args, **kwargs):
        self.matrix = RGBMatrix()
        self.board = [[(0, 0, 0) for i in range(32)] for i in range(32)]    #represents led matrix
        self.offset_canvas = self.matrix.CreateFrameCanvas()
    
    '''
    Initialise the start up sequence for the LED matrix
    Parameters:
        text (bool) - True if you need welcoming text
    '''
    def start_up_sequence(self, text):
        if text == True:
            # Write some welcoming text
            self.font = graphics.Font()
            self.font.LoadFont('/home/pi/rpi-rgb-led-matrix/fonts/9x18.bdf')
            textColour = graphics.Color(255, 255, 0)
            pos = self.offset_canvas.width 
            my_text = 'Welcome to Kratos-Red\'s 4011 Final Project :)'
            z = 1
            while True:
                z += 1
                if z == 440:
                    break
                self.offset_canvas.Clear()

                len = graphics.DrawText(self.offset_canvas, self.font, pos, 
                                10, textColour, my_text)
                pos -= 1
                if (pos + len < 0):
                    pos = self.offset_canvas.width
                time.sleep(0.025)
                self.offset_canvas = self.matrix.SwapOnVSync(self.offset_canvas)

        self.clear_board()  # Clear the board

        # Draw the square on the matrix
        self.fill_rectangle(8, 8, 16, 16, 255, 0, 0)
        self.fill_rectangle(8, 16, 16, 24, 255, 255, 0)
        self.fill_rectangle(16, 8, 24, 16, 0, 255, 0)
        self.fill_rectangle(16, 16, 24, 24, 0, 255, 255)

    '''
    Fill the LED matrix with a specified RGB value
    Parameters
        r, g, b (int) - RGB values
    '''
    def fill_board(self, r, g, b):
        for j in range(0, self.matrix.width):
            for i in range(0, self.matrix.height):
                self.board[i][j] = (r, g, b)
        self.update_matrix()

    '''
    Clear the board array
    '''
    def clear_board(self):
        self.fill_board(0, 0, 0)

    '''
    Update the LED matrix with the self.board array
    '''
    def update_matrix(self):
        for j in range(0, self.matrix.width):
            for i in range(0, self.matrix.height):
                self.offset_canvas.SetPixel(i, j, self.board[i][j][0], self.board[i][j][1], self.board[i][j][2])
        self.offset_canvas = self.matrix.SwapOnVSync(self.offset_canvas)
    
    '''
    Update the LED matrix with another array representing the led matrix
    Parameters:
        boardCopy (2d array) - array representing matrix
    '''
    def update_matrix_with_board(self, boardCopy):
        for j in range(0, self.matrix.width):
            for i in range(0, self.matrix.height):
                self.offset_canvas.SetPixel(i, j, boardCopy[i][j][0], boardCopy[i][j][1], boardCopy[i][j][2])
        self.offset_canvas = self.matrix.SwapOnVSync(self.offset_canvas)

    '''
    Fill a rectangle on the led matrix
    Parameters
        x1, x2 - x coordinates to fill
        y1, y2 - y coordinates to fill
        r, g, b - rgb values
    '''
    def fill_rectangle(self, x1, y1, x2, y2, r, g, b):
        for j in range(x1, x2):
            for i in range(y1, y2):
                self.board[i][j] = (r, g, b)
        self.update_matrix()    
    
    '''
    Flip the board on itself
    '''
    def flip_board(self):

        # Make a copy of the board
        flippedBoard = copy.deepcopy(self.board)
        for i in range(0, 32):
            for j in range(0, 15):
                flippedBoard[i][16 + j] = self.board[i][15 - j]
                flippedBoard[i][15 - j] = self.board[i][16 + j]

        # Complete the first part of the flip
        for frame in range(16):
            for i in range(0, 32):
                for j in range(0, 15):
                    self.board[i][16 + j] = self.board[i][16 + j + 1]
                    self.board[i][15 - j] = self.board[i][15 - j - 1]
            self.update_matrix()
            time.sleep(0.02)

        # Complete the second part of the flip
        iterations = 1
        for frame in range(16):
            for i in range(0, 32):
                for j in range(0, iterations):
                    self.board[i][15 - j] = flippedBoard[i][iterations - 1 - j]
                    self.board[i][16 + j] = flippedBoard[i][32 - iterations + j]
            iterations = iterations + 1
            self.update_matrix()
            time.sleep(0.02)

    '''
    Move the board in a specified direction by an amount of pixels
    Parameters:
        direction - direction of movement
        pixels - number of pixels to move by
    '''
    def move_array(self, direction, pixels):
        num_rows = len(self.board)
        num_cols = len(self.board[0])

        if direction == "up":
            for _ in range(pixels):
                self.board.insert(0, self.board.pop(num_rows - 1))
        elif direction == "down":
            for _ in range(pixels):
                self.board.append(self.board.pop(0))
        elif direction == "left":
            for row in self.board:
                for _ in range(pixels):
                    row.insert(0, row.pop(num_cols - 1))
        elif direction == "right":
            for row in self.board:
                for _ in range(pixels):
                    row.append(row.pop(0))
        self.update_matrix()
    
    '''
    Rotate the array by a given angle over a certain amount of frames
    Parameters:
        angle - angle of rotation
        frames - number of frames
    '''
    def rotate_array_by_angle(self, angle, frames):

        boardRotated = [[(0, 0, 0) for i in range(32)] for i in range(32)]

        cent_x = 31 / 2
        cent_y = 31 / 2

        rotate_square = min(31, 31) * 1.41
        min_rotate = cent_x - rotate_square / 2
        max_rotate = cent_x + rotate_square / 2

        display_square = min(31, 31) * 0.7
        min_display = cent_x - display_square / 2
        max_display = cent_x + display_square / 2

        deg_to_rad = 2 * 3.14159265 / 360
        rotation = 0
        stopCondition = copy.deepcopy(angle)
        framesCount = 0

        while True:

            # Check for stop condition
            if stopCondition > 0:
                rotation += 2
            else:
                rotation -= 2

            # See if we need to update the led matrix
            if framesCount == frames:  
                self.update_matrix_with_board(boardRotated)
                framesCount = 0
            if stopCondition == rotation:
                self.update_matrix_with_board(boardRotated)
                self.board = copy.deepcopy(boardRotated)
                return

            angle = rotation * deg_to_rad
            sin = math.sin(angle)
            cos = math.cos(angle)

            # Iterate through board and apply rotation matrix as necessary
            for x in range(int(min_rotate), int(max_rotate)):
                for y in range(int(min_rotate), int(max_rotate)):
                    rot_x, rot_y = rotate(x - cent_x, y - cent_x, sin, cos)

                    if x >= min_display and x < max_display and y >= min_display and y < max_display:
                        boardRotated[round(rot_x + cent_x)][round(rot_y + cent_y)] = self.board[x][y]    
                    else:
                        try:
                            boardRotated[round(rot_x + cent_x)][round(rot_y + cent_y)] = (0, 0, 0)
                        except IndexError:
                            continue

            framesCount = framesCount + 1   # Increment frames

'''
SerialCommunication: represents the serial communication between pc and nRF dongle
'''
class SerialCommunication(object):

    '''
    SerialCommunication constructor
    '''
    def __init__(self, *args, **kwargs):
        self.attempt_serial_connection()
    
    '''
    Attempt to write data to the serial port
    Parameters
        data - data to write
    '''
    def write_data(self, data):
        try:
            self.ser.write(data.encode('utf-8'))
            return 0
        except Exception as e:
            print (e)
            return -1
    
    '''
    Attempt to open a serial connection
    '''
    def attempt_serial_connection(self):
        try:
            self.ser = serial.Serial('/dev/ttyACM0', baudrate=115200, timeout=1)
        except Exception as e:
            print(e)
            return
    
    '''
    Get any incoming data from the nRF dongle
    '''    
    def get_data(self):
        rxBuffer = ""
        dataList = DATALIST_EMPTY
        commandList = COMMANDLIST_EMPTY
        valueFlag = False
        readFlag = False
        input = INPUT_EMPTY
        inputIndex = 0

        # Enter loop
        while True:
            try:
                c = self.ser.read().decode('utf-8')
                if c == '':
                    continue
                if valueFlag == False:
                    dataList.pop(0)
                    dataList.insert(7, c)
                    commandList.pop(0)
                    commandList.insert(10, c)
                elif valueFlag == True:
                    if c == '\r' or c == '\n':  # Check for end of line characters
                        valueFlag = False
                        if inputIndex == 1:
                            readFlag = True
                        inputIndex = 1 - inputIndex
                        continue
                    input[inputIndex] += c
            except Exception as e:
                return -1
            
            # Check if data or command byte
            if ''.join(dataList) == '<Data>: ':
                valueFlag = True
            elif ''.join(commandList) =='<Command>: ':
                valueFlag = True

            if readFlag == True:
                copyInput = input.copy()
                input.clear()
                input.append('')
                input.append('')
                return copyInput

'''
Main loop - control the program flow
'''
def main():

    # Create class objects
    serial_communication = SerialCommunication()
    led_matrix = LEDMatrix()

    serial_communication.write_data('connect\n')    # Check if there is an existing bt connection

    while True:
        data = serial_communication.get_data()   #get data over serial port
        
        #parse the data
        for i in range(len(data)):
            data[i] = int(data[i])

        # Apply the right movements on the LED board
        if data[COMMAND] == GESTURE:
            if data[DATA] == RIGHT_SWIPE:
                led_matrix.rotate_array_by_angle(90, 10)
            elif data[DATA] == LEFT_SWIPE:
                led_matrix.rotate_array_by_angle(-90, 10)
            elif data[DATA] == UP_SWIPE:
                led_matrix.flip_board()
            elif data[DATA] == DOWN_SWIPE:
                led_matrix.flip_board()
            elif data[DATA] == MOVE_UP:
                led_matrix.move_array("up", 4)    
            elif data[DATA] == MOVE_LEFT:
                led_matrix.move_array("left", 4)    
            elif data[DATA] == MOVE_DOWN:
                led_matrix.move_array("down", 4)    
            elif data[DATA] == MOVE_RIGHT:
                led_matrix.move_array("right", 4)                           
        elif data[COMMAND] == RESET:
            led_matrix.start_up_sequence(text=False)
        elif data[COMMAND] == CLEAR:
            led_matrix.clear_board()
        elif data[COMMAND] == START_UP:
            led_matrix.start_up_sequence(text=True)

'''
Apply a rotation matrix to a x, y coordinate
Parameters:
    x, y - coordinates
    sin, cos - sin/cos of an angle
'''
def rotate(x, y, sin, cos):
    return x * cos - y * sin, x * sin + y * cos

# Start the program
if __name__ == "__main__":
    main()