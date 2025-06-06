from flask import Flask, request, jsonify
from flask_cors import CORS
import os, json, openai

app = Flask(__name__, static_folder='static', static_url_path='')
CORS(app)
FIFO = '/tmp/robot-cmd'
TELE_FILE = '/tmp/telemetry.json'

openai.api_key = os.getenv("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")

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
    返回最新 telemetry JSON:
      { "bat_v": 6.12, "speed": 123 }
    """
    try:
        with open(TELE_FILE) as f:
            data = json.load(f)
    except:
        data = {}
    return jsonify(data)

# ————————— 新增：语音识别 + GPT 回复 —————————
@app.route('/api/voice', methods=['POST'])
def api_voice():
    print("[DEBUG] /api/voice 路由被调用")

    # 1. 检查请求是否包含 audio 文件
    if 'audio' not in request.files:
        print("[ERROR] 请求中没有找到 audio 字段")
        return jsonify({'error': 'no audio file'}), 400

    audio_file = request.files['audio']
    print(f"[DEBUG] 收到音频:filename={audio_file.filename}, content_type={audio_file.content_type}")

    # 2. 将上传的文件保存到临时路径
    temp_path = '/tmp/voice_input.webm'
    try:
        audio_file.save(temp_path)
        print(f"[DEBUG] 音频已保存到 {temp_path}")
    except Exception as e:
        print(f"[ERROR] 保存音频文件失败：{e}")
        return jsonify({'error': f'Failed to save audio: {e}'}), 500

    # 3. 调用 Whisper 转写 (OpenAI Python SDK v1 方式)
    transcription = None
    try:
        # 创建一个新的 OpenAI 客户端实例（v1 推荐用法）
        client = openai.OpenAI()
        print("[DEBUG] 正在使用 client.audio.transcriptions.create 进行 Whisper 转写...")
        with open(temp_path, 'rb') as audio_fp:
            transcript_resp = client.audio.transcriptions.create(
                model="whisper-1",
                file=audio_fp,
                language="zh",
            )
        # 新版接口返回的 JSON 中，用 "text" 字段存放转写结果
        transcription = transcript_resp.text
        print(f"[DEBUG] Whisper 转写结果：{transcription}")
    except Exception as e:
        print(f"[ERROR] Whisper 转写失败：{e}")
        # 如果想看更详细的异常堆栈，可改为 print(traceback.format_exc())
        return jsonify({'error': f'Whisper 识别失败: {e}'}), 500

    # 4. 调用 GPT-4o-mini（聊天接口）生成回复
    response_text = None
    try:
        print(f"[DEBUG] 开始调用 GPT 生成回复，输入文本：{transcription}")
        chat_resp = client.chat.completions.create(
            model="gpt-4o-mini",
            messages=[
                {"role": "system", "content": "You are a helpful robot assistant. Respond to the user's voice input. The User would tell you thins like Go forward, turn left, etc. You should reply with a single character command: 'w' for forward, 's' for backward, 'a' for left, 'd' for right, and 'p' for stop. Do not reply with any other text. Each command should be a single character follow by a Number which stands for Duration and a p for stop. For example, 'w5 p' means move forward for 5 seconds and automatically stop."},
                {"role": "user",   "content": transcription}
            ],
            temperature=0.7,
            max_tokens=150
        )
        response_text = chat_resp.choices[0].message.content.strip()
        print(f"[DEBUG] GPT 回复：{response_text}")
    except Exception as e:
        print(f"[ERROR] GPT ChatCompletion 失败：{e}")
        return jsonify({'error': f'ChatCompletion 失败: {e}'}), 500

    # 5. 返回结果给前端
    print("[DEBUG] 即将返回 JSON 响应给客户端")
    return jsonify({
        "transcription": transcription,
        "response": response_text
    }), 200

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=9001)
