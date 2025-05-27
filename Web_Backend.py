from flask import Flask, request, jsonify
import os, json

app = Flask(__name__)
FIFO = '/tmp/robot-cmd'
TELE_FILE = '/tmp/telemetry.json'

# 确保 FIFO 存在
if not os.path.exists(FIFO):
    os.mkfifo(FIFO)

@app.route('/api/command', methods=['POST'])
def api_command():
    data = request.get_json(force=True)
    ch = data.get('cmd')
    if not isinstance(ch, str) or len(ch) != 1:
        return ('Bad cmd', 400)
    with open(FIFO, 'w') as fifo:
        fifo.write(ch)
    return ('', 204)

@app.route('/api/telemetry', methods=['GET'])
def api_telemetry():
    """
    返回最新 telemetry JSON：
      { "bat_v": 6.12, "speed": 123 }
    """
    try:
        with open(TELE_FILE) as f:
            data = json.load(f)
    except:
        data = {}
    return jsonify(data)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=9001)
