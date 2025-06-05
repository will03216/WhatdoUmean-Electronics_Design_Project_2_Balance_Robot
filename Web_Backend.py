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
    """
    接收前端上传的音频文件(WebM/ogg/mp3/...),
    1. 用 OpenAI Whisper 模型做语音转文本
    2. 用 GPT 模型(chat_completion)生成回复
    3. 返回 JSON { transcription: "...", response: "..." }
    """
    # 1. 保存上传的音频到临时文件
    if 'audio' not in request.files:
        return jsonify({'error': 'no audio file'}), 400
    audio_file = request.files['audio']
    temp_path = '/tmp/voice_input.webm'
    audio_file.save(temp_path)

    # 2. 调用 Whisper 转写
    try:
        # model 可以指定 "whisper-1"
        transcript_resp = openai.Audio.transcribe(
            model="whisper-1",
            file=open(temp_path, 'rb')
        )
        transcription = transcript_resp['text']
    except Exception as e:
        return jsonify({'error': f'Whisper 识别失败: {e}'}), 500

    # 3. 调用 GPT 生成对话回复
    try:
        chat_resp = openai.ChatCompletion.create(
            model="gpt-4o-mini",
            messages=[
                {"role": "system", "content": "You are a helpful robot assistant."},
                {"role": "user", "content": transcription}
            ],
            temperature=0.7,
            max_tokens=150
        )
        response_text = chat_resp.choices[0].message.content.strip()
    except Exception as e:
        return jsonify({'error': f'ChatCompletion 失败: {e}'}), 500

    # 4. 返回前端
    return jsonify({
        "transcription": transcription,
        "response": response_text
    }), 200

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=9001)
