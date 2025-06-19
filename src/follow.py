import cv2, serial, time, torch, os, numpy as np
from pathlib import Path

import subprocess



def get_frame():
    result = subprocess.run(
        ["libcamera-still", "-n", "-t", "1", "--width", "320", "--height", "240", "-o", "-"],
        stdout=subprocess.PIPE
    )
    image = np.frombuffer(result.stdout, dtype=np.uint8)
    frame = cv2.imdecode(image, cv2.IMREAD_COLOR)
    return frame

# ========= USER SETTINGS =========
SERIAL_PORT  = "/dev/ttyUSB0"   # or "/dev/ttyAMA0" if you use GPIO14/15
BAUD_RATE    = 115200
FRAME_W, FRAME_H = 320, 240
CENTER_TOL   = 0.15             # ±15 % of image width = "centered"
FWD_CMD      = b"w"
LEFT_CMD     = b"a"
RIGHT_CMD    = b"d"
STOP_CMD     = b"p"
# =================================

# ----- open serial -----
ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
print(f"[Serial] opened {SERIAL_PORT}")

# ----- load YOLOv5 nano (fastest) -----
model = torch.hub.load("ultralytics/yolov5", "yolov5n", force_reload=False)
model.conf = 0.3            # confidence threshold
model.classes = [0]         # only detect 'person' (class 0)

# ----- open camera -----
# cap = cv2.VideoCapture(0, cv2.CAP_V4L2)
# cap.set(cv2.CAP_PROP_FRAME_WIDTH,  FRAME_W)
# cap.set(cv2.CAP_PROP_FRAME_HEIGHT, FRAME_H)

def send(cmd: bytes):
    """Send one‐byte command if it differs from the last one."""
    if not hasattr(send, "last") or send.last != cmd:
        ser.write(cmd)
        send.last = cmd
        print(f"[TX] {cmd.decode()}")

try:
    while True:
        ok, frame = get_frame()
        if not ok: continue

        # YOLO inference (single frame)
        results = model(frame, size=FRAME_W, augment=False)
        det = results.xyxy[0]            # (x1,y1,x2,y2,conf,cls)

        if det.shape[0]:
            # choose the largest person (max box area)
            areas = (det[:,2]-det[:,0]) * (det[:,3]-det[:,1])
            idx   = torch.argmax(areas)
            x1, y1, x2, y2, conf, cls = det[idx]
            center_x = ((x1 + x2) / 2).item()

            norm_x = center_x / FRAME_W  # 0.0 … 1.0
            offset = norm_x - 0.5        # −0.5 … +0.5

            if   offset < -CENTER_TOL: send(LEFT_CMD)
            elif offset >  CENTER_TOL: send(RIGHT_CMD)
            else:                       send(FWD_CMD)
        else:
            send(STOP_CMD)              # no person

        # optional: press q to quit / show live view
        cv2.imshow("view", frame)
        if cv2.waitKey(1) & 0xFF == ord('q'): break

except KeyboardInterrupt:
    pass
finally:
    send(STOP_CMD)
    # cap.release()
    ser.close()
    cv2.destroyAllWindows()