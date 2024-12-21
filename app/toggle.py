#! /usr/bin/python3

import serial
import sys
import time

ser = serial.Serial(sys.argv[1], 115200)
# Set DTR Low
ser.dtr = False
time.sleep(0.5)
# Set DTR High
ser.dtr = True
time.sleep(0.5)
# Close the port
ser.close()
