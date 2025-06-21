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
    return newest telemetry JSON:
      { "bat_v": 6.12, "speed": 123 }
    """
    try:
        with open(TELE_FILE) as f:
            data = json.load(f)
    except:
        data = {}
    return jsonify(data)

# voice transcription and command generation
@app.route('/api/voice', methods=['POST'])
def api_voice():
    print("[DEBUG] /api/voice is called starting to process...")

    # detect audio file in request
    if 'audio' not in request.files:
        print("[ERROR] No audio file found in request")
        return jsonify({'error': 'no audio file'}), 400

    audio_file = request.files['audio']
    print(f"[DEBUG] Received Audio:filename={audio_file.filename}, content_type={audio_file.content_type}")

    # save the audio file to a temporary location
    temp_path = '/tmp/voice_input.webm'
    try:
        audio_file.save(temp_path)
        print(f"[DEBUG] voice input saved to {temp_path}")
    except Exception as e:
        print(f"[ERROR] voice input save failed : {e}")
        return jsonify({'error': f'Failed to save audio: {e}'}), 500

    # use OpenAI Whisper to transcribe the audio
    transcription = None
    try:
        client = openai.OpenAI()
        print("[DEBUG] Using client.audio.transcriptions.create to have Whisper process...")
        with open(temp_path, 'rb') as audio_fp:
            transcript_resp = client.audio.transcriptions.create(
                model="whisper-1",
                file=audio_fp,
                language="en",
            )
        transcription = transcript_resp.text
        print(f"[DEBUG] Whisper Result: {transcription}")
    except Exception as e:
        print(f"[ERROR] Whisper transcript failed: {e}")
        return jsonify({'error': f'Whisper recognition failed: {e}'}), 500

    # use OpenAI GPT to generate commands based on the transcription
    response_text = None
    try:
        print(f"[DEBUG] Starting to use GPT, input text:{transcription}")
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
        print(f"[DEBUG] GPT Response: {response_text}")
        print("[DEBUG] Voice output incoming")
        # tts = gTTS(text=response_text, lang='en', tld="co.uk")
        # tts.save('/tmp/response.mp3')
        # os.system('ffplay -nodisp -autoexit /tmp/response.mp3')
    except Exception as e:
        print(f"[ERROR] GPT ChatCompletion failed: {e}")
        return jsonify({'error': f'ChatCompletion failed: {e}'}), 500
	
    # 5. analyze the GPT response to extract commands and durations
    # findall command pairs and duration
    pairs = re.findall(r'([wasd])(\d+)', response_text)
    if not pairs:
        print("[ERROR] 无法解析 GPT 输出 can not find command pairs")
        return jsonify({'error': 'invalid command format'}), 400

    for cmd_char, dur_str in pairs:
        dur = int(dur_str)
        print(f"[DEBUG] Command {cmd_char} is detected, for {dur} seconds")
        try:
            with open(FIFO, 'w') as fifo:
                fifo.write(cmd_char)
            print(f"[DEBUG] write into FIFO: {cmd_char}")
        except Exception as e:
            print(f"[ERROR] FIFO writing failed: {e}")
        time.sleep(dur)

    try:
        with open(FIFO, 'w') as fifo:
            fifo.write('p')
        print("[DEBUG] Writing FIFO: p (stop)")
    except Exception as e:
        print(f"[ERROR] Writing Stop p to FIFO failed: {e}")

    # return to frontend
    print("[DEBUG] JSON response to client")    
    return jsonify({
        "transcription": transcription,
        "response": response_text
    }), 200

if __name__ == '__main__':
    ssl_cert = "/home/zhang/WebUI/certs/server-cert.pem"
    ssl_key  = "/home/zhang/WebUI/certs/server-key.pem"
    print(f"⚡️ 启动 HTTPS Flask → port=9001, cert={ssl_cert}, key={ssl_key}")
    app.run( host='0.0.0.0', port=9001, ssl_context=(ssl_cert, ssl_key), debug=True)




