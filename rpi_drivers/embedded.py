import serial
import sys
import math
import time
import copy

sys.path.append('/home/pi/rpi-rgb-led-matrix/bindings/python')
from rgbmatrix import RGBMatrix, RGBMatrixOptions, graphics
from samples.samplebase import SampleBase

DATALIST_EMPTY = ['', '', '', '', '', '', '', '']
COMMANDLIST_EMPTY = ['', '', '', '', '', '', '', '', '', '', '']
INPUT_EMPTY = ['', '']

def scale_col(val, lo, hi):
    if val < lo:
        return 0
    if val > hi:
        return 255
    return 255 * (val - lo) / (hi - lo)


def rotate(x, y, sin, cos):
    return x * cos + y * sin, x * sin - y * cos


class RotatingBlockGenerator(object):
    def __init__(self, *args, **kwargs):
        self.matrix = RGBMatrix()
        self.board = [[(0, 0, 0) for i in range(32)] for i in range(32)]
        self.offset_canvas = self.matrix.CreateFrameCanvas()
        
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

        # Draw the square on the matrix
        self.fill_rectangle(8, 8, 16, 16, 255, 0, 0)
        self.fill_rectangle(8, 16, 16, 24, 255, 255, 0)
        self.fill_rectangle(16, 8, 24, 16, 0, 255, 0)
        self.fill_rectangle(16, 16, 24, 24, 0, 255, 255)    
        

        time.sleep(5)
        self.flip_board()

    def fill_board(self, r, g, b):
        for j in range(0, self.matrix.width):
            for i in range(0, self.matrix.height):
                self.board[i][j] = (r, g, b)
        self.update_matrix()

    def clear_board(self):
        for j in range(x1, x2):
            for i in range(y1, y2):
                self.board[i][j] = (0, 0, 0)
        self.update_matrix()

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
    
    def flip_board(self):
        boardFlip = copy.deepcopy(self.board)
        for i in range(0, self.matrix.width):
            self.board[i].reverse()
        
        iterations = 16
        while iterations > 1:
            boardFlip[iterations - 1] = copy.deepcopy(boardFlip[iterations - 1 - 1])
            boardFlip[self.matrix.width - iterations] = copy.deepcopy(boardFlip[self.matrix.width - iterations])
            iterations = iterations - 1

            self.update_matrix_with_board(boardFlip)
            time.sleep(0.5)


        self.update_matrix()

'''
Main loop - control the program flow
'''
def main():

    # Attempt to open serial port
    try:
        ser = serial.Serial('/dev/ttyACM0', baudrate=115200, timeout=1)
    except Exception as e:
        print(e)
        return
    
    rotating_block_generator = RotatingBlockGenerator()
    
    while True:
        data = get_data(ser)   #get data over serial port
        rotating_block_generator.process()
    

'''
Get any incoming data from the nRF dongle
@param ser serial port object
'''    
def get_data(ser):
    rxBuffer = ""
    dataList = DATALIST_EMPTY
    commandList = COMMANDLIST_EMPTY
    valueFlag = False
    readFlag = False
    input = INPUT_EMPTY
    inputIndex = 0
    while True:
        try:
            c = ser.read().decode('utf-8')
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
            print('Got values: ', input)
            copyInput = input.copy()
            input.clear()
            input.append('')
            input.append('')
            return copyInput

'''
Start the program
'''
if __name__ == "__main__":
    main()