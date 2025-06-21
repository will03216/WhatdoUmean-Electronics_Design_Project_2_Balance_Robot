# redirect_service.py
from flask import Flask, jsonify, send_from_directory, request

app = Flask(__name__, static_folder='static', static_url_path='')

is_connected = False


TARGET_URL = "https://whatdoumeanrobot.com/Web_UI.html/"
CHECK_HOST = "127.0.0.1"
CHECK_PORT = 60001
CHECK_TIMEOUT = 1  

def is_port_open(host: str, port: int, timeout: int = CHECK_TIMEOUT) -> bool:
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
    return jsonify({'connected': is_connected})

@app.route('/set_connected', methods=['POST'])
def set_connected():
    global is_connected
    is_connected = True
    return jsonify({'success': True})

@app.route('/')
def index():
    return send_from_directory(app.static_folder, 'index.html')

if __name__ == '__main__':
    context = ('/home/ubuntu/cert/server.crt', '/home/ubuntu/cert/server.key')
    app.run(host='0.0.0.0', port=443, ssl_context=context)
