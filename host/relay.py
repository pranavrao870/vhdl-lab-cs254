import serial
import sys

if __name__ == '__main__':
    ser0 = serial.Serial(sys.argv[1], baudrate=115200, timeout = 0.0001)
    ser1 = serial.Serial(sys.argv[2], baudrate=115200, timeout = 0.0001)
    while True:
        recv0 = ser0.read()
        if not (recv0 == b''):
            print("Got 0")
            print(recv0.hex())
            ser1.write(recv0)
        recv1 = ser1.read()
        if not (recv1 == b''):
            print("Got 1")
            print(recv1.hex())
            ser0.write(recv1)