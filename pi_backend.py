import os, json, time, serial, threading

FIFO_CMD   = '/tmp/robot-cmd'
TELE_FILE  = '/tmp/telemetry.json'
BAUDRATE   = 115200
PORT       = '/dev/ttyUSB0'

# open serial port
ser = serial.Serial(PORT, BAUDRATE, timeout=1)

# ensure FIFO exists
if not os.path.exists(FIFO_CMD):
    os.mkfifo(FIFO_CMD)

def telemetry_loop():
    while True:
        line = ser.readline().decode('utf8', errors='ignore').strip()
        if not line:
            time.sleep(0.01)
            continue
        try:
            msg = json.loads(line)
            tele = msg.get('tele', {})
        except:
            continue
        tmp = TELE_FILE + '.tmp'
        with open(tmp, 'w') as f:
            json.dump(tele, f)
        os.replace(tmp, TELE_FILE)


t = threading.Thread(target=telemetry_loop, daemon=True)
t.start()

def command_loop():
    while True:
        with open(FIFO_CMD, 'r') as fifo:
            for ch in fifo.read(): 
                if not ch:
                    continue
                ser.write(ch.encode('utf8'))
        time.sleep(0.005)

if __name__ == '__main__':
    command_loop()
