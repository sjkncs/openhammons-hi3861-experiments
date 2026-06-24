"""
ZhiPu LLM Handler

Functions:
  - ask_query(shadow_data, user_input): Generate natural language answer from device shadow
  - parse_command(user_input): Parse user text into structured JSON command
"""

import json
from config import ZHIPU_API_KEY

try:
    from zhipuai import ZhipuAI
    client = ZhipuAI(api_key=ZHIPU_API_KEY) if ZHIPU_API_KEY else None
except ImportError:
    client = None
    print("[LLM] WARNING: zhipuai not installed. Run: pip install zhipuai")


def ask_query(shadow_data, user_input):
    """Given device shadow data and user question, generate natural language answer.

    Args:
        shadow_data: dict from IoTDA shadow API
        user_input: user question string

    Returns:
        Natural language answer string
    """
    if client is None:
        # Fallback: extract data directly
        return _fallback_query(shadow_data)

    shadow_str = json.dumps(shadow_data, ensure_ascii=False, indent=2)

    prompt = f"""You are an IoT assistant. Based on the following device shadow data, answer the user's question in Chinese.
Be concise and natural. If the data contains temperature, humidity, or gas concentration, include the values.

Device Shadow Data:
{shadow_str}

User Question: {user_input}

Answer (in Chinese, concise):"""

    try:
        response = client.chat.completions.create(
            model="glm-4-flash",
            messages=[{"role": "user", "content": prompt}],
            temperature=0.3,
            max_tokens=200,
        )
        return response.choices[0].message.content
    except Exception as e:
        print(f"[LLM] Query error: {e}")
        return _fallback_query(shadow_data)


def _fallback_query(shadow_data):
    """Fallback: extract values from shadow without LLM."""
    try:
        props = {}
        for svc in shadow_data.get("shadow", []):
            if svc.get("service_id") == "AirEnvironment":
                reported = svc.get("reported_properties", {})
                props.update(reported.get("properties", {}))

        parts = []
        if "temperature" in props:
            parts.append(f"Temperature: {props['temperature']} C")
        if "humidity" in props:
            parts.append(f"Humidity: {props['humidity']}%")
        if "gas_concentration" in props:
            parts.append(f"Gas: {props['gas_concentration']}")

        if parts:
            return "Current device status: " + ", ".join(parts)
        else:
            return "No sensor data available. Please check if the device is online."
    except Exception:
        return "Unable to parse device data."


def parse_command(user_input):
    """Parse user natural language into structured JSON command.

    Args:
        user_input: user text like "turn on the light" or "buzzer flash"

    Returns:
        dict like {"led": "ON"} or {"buzzer": "FLASH"}, or None if not a command
    """
    if client is None:
        return _fallback_parse(user_input)

    prompt = f"""You are an IoT command parser. Analyze the user input and determine if it's a device control command.

Valid commands:
- LED: {{"led": "ON"}} or {{"led": "OFF"}}
- Buzzer: {{"buzzer": "ALWAYS"}} or {{"buzzer": "FLASH"}} or {{"buzzer": "OFF"}}

If the input is a control command, respond with ONLY the JSON (no other text).
If it's NOT a command (e.g., a question), respond with "NOT_COMMAND".

User input: {user_input}

Response (JSON only or NOT_COMMAND):"""

    try:
        response = client.chat.completions.create(
            model="glm-4-flash",
            messages=[{"role": "user", "content": prompt}],
            temperature=0.1,
            max_tokens=50,
        )
        result = response.choices[0].message.content.strip()

        if result == "NOT_COMMAND" or result.startswith("NOT"):
            return None

        # Try to extract JSON
        if "{" in result:
            start = result.index("{")
            end = result.rindex("}") + 1
            cmd = json.loads(result[start:end])
            # Validate
            if "led" in cmd and cmd["led"] in ("ON", "OFF"):
                return cmd
            if "buzzer" in cmd and cmd["buzzer"] in ("ALWAYS", "FLASH", "OFF"):
                return cmd
        return None
    except Exception as e:
        print(f"[LLM] Parse error: {e}")
        return _fallback_parse(user_input)


def _fallback_parse(user_input):
    """Simple keyword-based fallback command parsing."""
    text = user_input.lower()

    # LED commands
    if any(w in text for w in ["open light", "turn on light", "open lamp", "light on"]):
        return {"led": "ON"}
    if any(w in text for w in ["close light", "turn off light", "close lamp", "light off"]):
        return {"led": "OFF"}

    # Buzzer commands
    if any(w in text for w in ["open buzzer", "buzzer on", "buzzer always"]):
        return {"buzzer": "ALWAYS"}
    if any(w in text for w in ["buzzer flash", "buzzer intermittent", "flash alarm"]):
        return {"buzzer": "FLASH"}
    if any(w in text for w in ["close buzzer", "buzzer off", "stop buzzer"]):
        return {"buzzer": "OFF"}

    return None
