from flask import Flask, request, jsonify
from flask_cors import CORS
import os, json, openai, re, time
from gtts import gTTS

with open("gpt_prompt.txt", "r", encoding="utf-8") as f:
    gpt_prompt = f.read()
    
	
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
        # 创建一个新的 OpenAI 客户端实例
        client = openai.OpenAI()
        print("[DEBUG] 正在使用 client.audio.transcriptions.create 进行 Whisper 转写...")
        with open(temp_path, 'rb') as audio_fp:
            transcript_resp = client.audio.transcriptions.create(
                model="whisper-1",
                file=audio_fp,
                language="en",
            )
        # 新版接口返回的 JSON 中，用 "text" 字段存放转写结果
        transcription = transcript_resp.text
        print(f"[DEBUG] Whisper 转写结果：{transcription}")
    except Exception as e:
        print(f"[ERROR] Whisper 转写失败：{e}")
        return jsonify({'error': f'Whisper 识别失败: {e}'}), 500

    # 4. 调用 GPT-4o-mini（聊天接口）生成回复
    response_text = None
    try:
        print(f"[DEBUG] 开始调用 GPT 生成回复，输入文本：{transcription}")
        chat_resp = client.chat.completions.create(
            model="gpt-4.1-nano",
            messages=[
                {"role": "system", "content": gpt_prompt},
                {"role": "user",   "content": transcription}
            ],
            temperature=1,
            max_tokens=150
        )
        response_text = chat_resp.choices[0].message.content.strip()
        print(f"[DEBUG] GPT 回复：{response_text}")
        print("[DEBUG] 即将输出语音回复")
        # tts = gTTS(text=response_text, lang='en', tld="co.uk")
        # tts.save('/tmp/response.mp3')
        # os.system('ffplay -nodisp -autoexit /tmp/response.mp3')
    except Exception as e:
        print(f"[ERROR] GPT ChatCompletion 失败：{e}")
        return jsonify({'error': f'ChatCompletion 失败: {e}'}), 500
	
    # 5. 解析 GPT 回复，提取命令字符和时长 发给 ESP32
    # 解析出命令字符和时长
    pairs = re.findall(r'([wasd])(\d+)', response_text)
    if not pairs:
        print("[ERROR] 无法解析 GPT 输出")
        return jsonify({'error': 'invalid command format'}), 400
    
    # 依次遍历每对 (cmd_char, dur_str)
    for cmd_char, dur_str in pairs:
        dur = int(dur_str)
        print(f"[DEBUG] 解析到命令 {cmd_char}，持续 {dur} 秒")
        # 写入运动命令
        try:
            with open(FIFO, 'w') as fifo:
                fifo.write(cmd_char)
            print(f"[DEBUG] 写入 FIFO: {cmd_char}")
        except Exception as e:
            print(f"[ERROR] 写命令到 FIFO 失败：{e}")
        # 等待对应时长
        time.sleep(dur)

    try:
        with open(FIFO, 'w') as fifo:
            fifo.write('p')
        print("[DEBUG] 写入 FIFO: p (停止)")
    except Exception as e:
        print(f"[ERROR] 写停止到 FIFO 失败：{e}")

    # 返回结果给前端
    print("[DEBUG] 即将返回 JSON 响应给客户端")    
    return jsonify({
        "transcription": transcription,
        "response": response_text
    }), 200

if __name__ == '__main__':
    # SSL 证书和私钥的绝对路径
    ssl_cert = "/home/zhang/WebUI/certs/server-cert.pem"
    ssl_key  = "/home/zhang/WebUI/certs/server-key.pem"
    print(f"⚡️ 启动 HTTPS Flask → port=9001, cert={ssl_cert}, key={ssl_key}")
    app.run( host='0.0.0.0', port=9001, ssl_context=(ssl_cert, ssl_key), debug=True)

