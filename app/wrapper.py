#! /usr/bin/python3

# this is a wrapper between the serial-device on which the uC listens on and UCI programs on a pc

import os
import select
import serial
import sys
import threading
import time


start = time.time()

def ser2stdout(ser, fh):
    fd_ser = ser.fileno()

    while True:
        try:
            while True:
                readable, writable, exceptional = select.select([fd_ser], [], [])
                for r in readable:
                    if fd_ser == r:
                        new_data = ser.read_all().decode('ascii')
                        fh.write(f"{int((time.time() - start) * 1000) / 1000} {new_data.replace('\r', '')}")
                        fh.flush()
                        print(new_data.replace('\r', ''), end='', flush=True)
        except Exception as e:
            fh.write(f'ser2stdout exception: {e}\n')
            print(str(e), file=sys.stderr, flush=True)
            time.sleep(0.1)


def stdin2ser(ser, fh):
    while True:
        try:
            data = ''
            while True:
                new_data = os.read(0, 4096)
                fh.write(f"{int((time.time() - start) * 1000) / 1000} {new_data.decode('ascii')}")
                fh.flush()
                data += new_data.decode('ascii')
                if 'quit\r\n' in data or 'quit\n' in data:
                    return
                if '\r\n' in data or '\n' in data:
                    data = ''
                bla = new_data
                while len(bla) > 0:
                    q = len(bla[0:10])
                    if ser.write(bla[0:10]) != q:
                        fh.write('DATA MISSING\n');
                    ser.flush()
                    bla = bla[q:]
                    time.sleep(0.01)
        except Exception as e:
            fh.write(f'stdin2ser exception: {e}\n')
            print(str(e), file=sys.stderr, flush=True)
            time.sleep(0.1)


fh = open('/tmp/log-dog.dat', 'a+')

ser = serial.Serial(sys.argv[1], 115200, exclusive=True)

tr = threading.Thread(target=ser2stdout, args=(ser, fh))
tr.daemon = True
tr.start()

tt = threading.Thread(target=stdin2ser, args=(ser, fh))
tt.start()
tt.join()

sys.exit(0)
