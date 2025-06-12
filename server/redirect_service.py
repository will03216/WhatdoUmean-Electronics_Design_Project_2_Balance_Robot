# redirect_service.py
from flask import Flask, jsonify, send_from_directory, request

app = Flask(__name__, static_folder='static', static_url_path='')

# 用于存储当前连接状态
is_connected = False


# —— 配置区 —— 
# 按需改成你的跳转目标
TARGET_URL = "https://whatdoumeanrobot.com/Web_UI.html/"
# 要检测的主机和端口（比如你的服务或 FRP 暴露的端口）
CHECK_HOST = "127.0.0.1"
CHECK_PORT = 60001
CHECK_TIMEOUT = 1  # 秒

# —— 端口检测函数 —— 
def is_port_open(host: str, port: int, timeout: int = CHECK_TIMEOUT) -> bool:
    """返回 True: 目标端口可连; False: 不可连"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(timeout)
    try:
        sock.connect((host, port))
        return True
    except socket.error:
        return False
    finally:
        sock.close()


@app.route('/status')
def status():
    """
    返回 {'connected': true/false}
    前端每秒轮询此接口，根据状态切换 UI
    """
    return jsonify({'connected': is_connected})

@app.route('/set_connected', methods=['POST'])
def set_connected():
    """
    在后台检测到连接后，可由你的业务逻辑调用此接口
    （例如在 WebSocket on_open 里 POST 一次此接口）
    """
    global is_connected
    is_connected = True
    return jsonify({'success': True})

# 将所有静态文件（index.html、style.css、script.js）都放在 ./static 目录下
@app.route('/')
def index():
    return send_from_directory(app.static_folder, 'index.html')

if __name__ == '__main__':
    # 根据需求修改 host/port
    context = ('/home/ubuntu/cert/server.crt', '/home/ubuntu/cert/server.key')
    app.run(host='0.0.0.0', port=443, ssl_context=context)
