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
        fh = open('/tmp/log-dog.dat', 'a+')
        ser = serial.Serial(sys.argv[1], 115200, timeout=0)
#        ser.dtr = False
#        time.sleep(0.5)
#        ser.dtr = True
#        time.sleep(2.0)

        fd_ser = ser.fileno()
        data = ''
        start = time.time()
        while True:
            readable, writable, exceptional = select.select([fd_ser, 0], [], [])
            for r in readable:
                if r == 0:
                    new_data = os.read(0, 4096)
                    fh.write(f"{int((time.time() - start) * 1000) / 1000} {new_data.decode('ascii')}")
                    data += new_data.decode('ascii')
                    if 'quit\r\n' in data or 'quit\n' in data:
                        sys.exit(0)
                    if '\r\n' in data or '\n' in data:
                        data = ''
                    bla = new_data
                    while len(bla) > 0:
                        q = len(bla[0:10])
                        ser.write(bla[0:10])
                        bla = bla[q:]
                        time.sleep(0.05)
                elif r == fd_ser:
                    new_data = ser.read(4096).decode('ascii')
                    fh.write(f"{int((time.time() - start) * 1000) / 1000} {new_data.replace('\r', '')}")
                    print(new_data.replace('\r', ''), end='', flush=True)
                else:
                    print('Internal error')
                    sys.exit(1)
            fh.flush()

        fh.close()

    finally:
        if ser != None:
            ser.close()
