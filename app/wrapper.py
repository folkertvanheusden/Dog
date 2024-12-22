#! /usr/bin/python3

# this is a wrapper between the serial-device on which the uC listens on and UCI programs on a pc

import os
import select
import serial
import sys
import time

while True:
    ser = None
    try:
        ser = serial.Serial(sys.argv[1], 115200, timeout=0)
        ser.dtr = False
        time.sleep(0.5)
        ser.dtr = True
        time.sleep(0.5)

        fd_ser = ser.fileno()

        data = ''

        while True:
            readable, writable, exceptional = select.select([fd_ser, 0], [], [])
            for r in readable:
                if r == 0:
                    new_data = os.read(0, 1)
                    data += new_data.decode('ascii')
                    if 'quit\r\n' in data or 'quit\n' in data:
                        sys.exit(0)
                    if '\r\n' in data or '\n' in data:
                        data = ''
                    ser.write(new_data)
                elif r == fd_ser:
                    print(ser.read().decode('ascii'), end='', flush=True)
                else:
                    print('Internal error')
                    sys.exit(1)

    finally:
        if ser != None:
            ser.close()
