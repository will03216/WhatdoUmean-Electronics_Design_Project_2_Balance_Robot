import os, json, time

FIFO_CMD   = '/tmp/robot-cmd'

if not os.path.exists(FIFO_CMD):
    os.mkfifo(FIFO_CMD)

while True:
    with open(FIFO_CMD, 'r') as fifo:
        for ch in fifo.read(): 
            if not ch:
                continue
            print(ch)
    time.sleep(0.005)
