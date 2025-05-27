from flask import Flask, request
import os

app = Flask(__name__)
FIFO = '/tmp/robot-cmd'

# 首次启动：创建 FIFO
if not os.path.exists(FIFO):
    os.mkfifo(FIFO)

@app.route('/api/command', methods=['POST'])
def api_command():
    data = request.get_json(force=True)
    ch = data.get('cmd')  # 期望是单字符 'w','a','s','d' 或 'p'
    if not isinstance(ch, str) or len(ch) != 1:
        return ('Bad cmd', 400)
    # 写入 FIFO，Pi 后台会立即读取
    with open(FIFO, 'w') as fifo:
        fifo.write(ch)
    return ('', 204)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=9001)
