import serial
import sys
import math
import time
import copy

sys.path.append('/home/pi/rpi-rgb-led-matrix/bindings/python')
from rgbmatrix import RGBMatrix, RGBMatrixOptions, graphics

DATALIST_EMPTY = ['', '', '', '', '', '', '', '']
COMMANDLIST_EMPTY = ['', '', '', '', '', '', '', '', '', '', '']
INPUT_EMPTY = ['', '']

COMMAND = 0
DATA = 1

RIGHT_SWIPE = 1
LEFT_SWIPE = 2
UP_SWIPE = 3
DOWN_SWIPE = 4

GESTURE = 1
RESET = 2
CLEAR = 3

class RotatingBlockGenerator(object):
    def __init__(self, *args, **kwargs):
        self.matrix = RGBMatrix()
        self.board = [[(0, 0, 0) for i in range(32)] for i in range(32)]
        self.offset_canvas = self.matrix.CreateFrameCanvas()
    
    def start_up_sequence(self):
        # Write some welcoming text
        # self.font = graphics.Font()
        # self.font.LoadFont('/home/pi/rpi-rgb-led-matrix/fonts/9x18.bdf')
        # textColour = graphics.Color(255, 255, 0)
        # pos = self.offset_canvas.width 
        # my_text = 'Welcome to Kratos-Red\'s 4011 Final Project :)'
        # z = 1
        # while True:
        #     z += 1
        #     if z == 440:
        #         break
        #     self.offset_canvas.Clear()

        #     len = graphics.DrawText(self.offset_canvas, self.font, pos, 
        #                     10, textColour, my_text)
        #     pos -= 1
        #     if (pos + len < 0):
        #         pos = self.offset_canvas.width
        #     time.sleep(0.05)
        #     self.offset_canvas = self.matrix.SwapOnVSync(self.offset_canvas)

        self.clear_board()

        # Draw the square on the matrix
        self.fill_rectangle(8, 8, 16, 16, 255, 0, 0)
        self.fill_rectangle(8, 16, 16, 24, 255, 255, 0)
        self.fill_rectangle(16, 8, 24, 16, 0, 255, 0)
        self.fill_rectangle(16, 16, 24, 24, 0, 255, 255)


    def fill_board(self, r, g, b):
        for j in range(0, self.matrix.width):
            for i in range(0, self.matrix.height):
                self.board[i][j] = (r, g, b)
        self.update_matrix()

    def clear_board(self):
        self.fill_board(0, 0, 0)

    def update_matrix(self):
        for j in range(0, self.matrix.width):
            for i in range(0, self.matrix.height):
                self.offset_canvas.SetPixel(i, j, self.board[i][j][0], self.board[i][j][1], self.board[i][j][2])
        self.offset_canvas = self.matrix.SwapOnVSync(self.offset_canvas)
    
    def update_matrix_with_board(self, boardCopy):
        for j in range(0, self.matrix.width):
            for i in range(0, self.matrix.height):
                self.offset_canvas.SetPixel(i, j, boardCopy[i][j][0], boardCopy[i][j][1], boardCopy[i][j][2])
        self.offset_canvas = self.matrix.SwapOnVSync(self.offset_canvas)

    def fill_rectangle(self, x1, y1, x2, y2, r, g, b):
        for j in range(x1, x2):
            for i in range(y1, y2):
                self.board[i][j] = (r, g, b)
        self.update_matrix()    
    
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

        offset_canvas = self.matrix.CreateFrameCanvas()

        while True:
            if stopCondition > 0:
                rotation += 2
            else:
                rotation -= 2

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

            framesCount = framesCount + 1

class SerialCommunication(object):
    def __init__(self, *args, **kwargs):
        # Attempt to open serial port
        try:
            self.ser = serial.Serial('/dev/ttyACM0', baudrate=115200, timeout=1)
        except Exception as e:
            print(e)
            return
    
    '''
    Get any incoming data from the nRF dongle
    @param ser serial port object
    '''    
    def get_data(self):
        rxBuffer = ""
        dataList = DATALIST_EMPTY
        commandList = COMMANDLIST_EMPTY
        valueFlag = False
        readFlag = False
        input = INPUT_EMPTY
        inputIndex = 0
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
                    if c == '\r' or c == '\n':
                        valueFlag = False
                        if inputIndex == 1:
                            readFlag = True
                        inputIndex = 1 - inputIndex
                        continue
                    input[inputIndex] += c
            except Exception as e:
                print(e)
            
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

    serial_communication = SerialCommunication()
    rotating_block_generator = RotatingBlockGenerator()
    
    while True:
        data = serial_communication.get_data()   #get data over serial port
        for i in range(len(data)):
            data[i] = int(data[i])

        if data[COMMAND] == GESTURE:
            if data[DATA] == RIGHT_SWIPE:
                rotating_block_generator.rotate_array_by_angle(90, 10)
            elif data[DATA] == LEFT_SWIPE:
                rotating_block_generator.rotate_array_by_angle(-90, 10)
            elif data[DATA] == UP_SWIPE:
                rotating_block_generator.rotate_array_by_angle(180, 10)
            elif data[DATA] == DOWN_SWIPE:
                rotating_block_generator.rotate_array_by_angle(-180, 10)
        elif data[COMMAND] == RESET:
            rotating_block_generator.start_up_sequence()
        elif data[COMMAND] == CLEAR:
            rotating_block_generator.clear_board()

def rotate(x, y, sin, cos):
    return x * cos - y * sin, x * sin + y * cos

'''
Start the program
'''
if __name__ == "__main__":
    main()