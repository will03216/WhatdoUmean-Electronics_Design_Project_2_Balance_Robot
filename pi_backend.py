import os, json, time, serial, threading

FIFO_CMD   = '/tmp/robot-cmd'
TELE_FILE  = '/tmp/telemetry.json'
BAUDRATE   = 115200
PORT       = '/dev/ttyUSB0'

# 打开 ESP32 串口
ser = serial.Serial(PORT, BAUDRATE, timeout=1)

# 确保命名管道存在
if not os.path.exists(FIFO_CMD):
    os.mkfifo(FIFO_CMD)

# ---- 串口读线程：ESP32 --> Pi --> telemetry.json ----
def telemetry_loop():
    while True:
        line = ser.readline().decode('utf8', errors='ignore').strip()
        if not line:
            time.sleep(0.01)
            continue
        # 假设 ESP32 发来 {"tele":{"bat_v":6.1,"speed":123}}
        try:
            msg = json.loads(line)
            tele = msg.get('tele', {})
        except:
            continue
        # 原子写文件
        tmp = TELE_FILE + '.tmp'
        with open(tmp, 'w') as f:
            json.dump(tele, f)
        os.replace(tmp, TELE_FILE)

# 启动 telemetry 线程
t = threading.Thread(target=telemetry_loop, daemon=True)
t.start()

# ---- FIFO 读线程：前端 --> Pi --> ESP32 ----
def command_loop():
    while True:
        with open(FIFO_CMD, 'r') as fifo:
            for ch in fifo.read():  # 读单字符
                if not ch:
                    continue
                ser.write(ch.encode('utf8'))
        time.sleep(0.005)

# 启动命令循环
if __name__ == '__main__':
    command_loop()
