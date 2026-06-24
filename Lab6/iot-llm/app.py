"""
Lab6 Flask Web Backend (iot-llm)

Routes:
  GET  /      - Serve chat interface
  POST /chat  - Handle chat: query device shadow or send commands
"""

from flask import Flask, render_template, request, jsonify
from iotda_handler import get_device_shadow, send_command
from llm_handler import ask_query, parse_command
import json

app = Flask(__name__)

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/chat", methods=["POST"])
def chat():
    data = request.get_json()
    user_input = data.get("message", "").strip()
    if not user_input:
        return jsonify({"reply": "Please enter a message."})

    try:
        # Try to parse as command first
        cmd_json = parse_command(user_input)
        if cmd_json:
            # It is a control command - send to device
            result = send_command(cmd_json)
            if result:
                return jsonify({"reply": f"Command sent successfully: {json.dumps(cmd_json, ensure_ascii=False)}"})
            else:
                return jsonify({"reply": "Command send failed. Check device is online."})

        # Otherwise it is a query - get device shadow
        shadow = get_device_shadow()
        if shadow is None:
            return jsonify({"reply": "Failed to query device shadow."})

        # Use LLM to generate natural language answer
        answer = ask_query(shadow, user_input)
        return jsonify({"reply": answer})

    except Exception as e:
        return jsonify({"reply": f"Error: {str(e)}"})

if __name__ == "__main__":
    print("[iot-llm] Starting Flask server on http://127.0.0.1:5000")
    app.run(host="0.0.0.0", port=5000, debug=True)
