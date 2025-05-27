import os, time, serial

FIFO_CMD = '/tmp/robot-cmd'
PORT     = '/dev/ttyUSB0'  # 串口设备
BAUDRATE = 115200

# 打开 ESP32 串口
ser = serial.Serial(PORT, BAUDRATE, timeout=1)

# 确保 FIFO 存在
if not os.path.exists(FIFO_CMD):
    os.mkfifo(FIFO_CMD)

while True:
    # 阻塞式打开读端，收到一个字符就处理
    with open(FIFO_CMD, 'r') as fifo:
        ch = fifo.read(1)  # 读一个字符
        if not ch:
            continue
        # 直接写到串口
        ser.write(ch.encode('utf8'))
    # 写端关闭后重开
    time.sleep(0.005)
