"""
IoTDA OpenAPI Handler

Handles:
  - Device shadow query (GET /v5/iot/{project_id}/devices/{device_id}/shadow)
  - Command send (POST /v5/iot/{project_id}/devices/{device_id}/commands)

Uses HMAC-SHA256 signing with AK/SK for authentication.
"""

import hashlib
import hmac
import datetime
import requests
from config import (
    HUAWEI_AK, HUAWEI_SK, IOTDA_ENDPOINT, PROJECT_ID, DEVICE_ID,
    SERVICE_ID, COMMAND_NAME, IOTDA_INSTANCE_ID
)


def _sign(key, msg):
    return hmac.new(key, msg.encode("utf-8"), hashlib.sha256).digest()


def _get_signature_key(secret, date_stamp, region, service):
    k_date = _sign(secret.encode("utf-8"), date_stamp)
    k_region = _sign(k_date, region)
    k_service = _sign(k_region, service)
    k_signing = _sign(k_service, "aws4_request")
    return k_signing


def _build_headers(method, path, body=None):
    """Build signed request headers for IoTDA API."""
    t = datetime.datetime.utcnow()
    date_stamp = t.strftime("%Y%m%d")
    amz_date = t.strftime("%Y%m%dT%H%M%SZ")

    canonical_uri = path
    canonical_querystring = ""

    if body:
        payload = body if isinstance(body, str) else str(body)
    else:
        payload = ""

    payload_hash = hashlib.sha256(payload.encode("utf-8")).hexdigest()

    headers_to_sign = {
        "host": IOTDA_ENDPOINT,
        "x-sdk-date": amz_date,
        "x-sdk-content-sha256": payload_hash,
    }

    signed_header_keys = sorted(headers_to_sign.keys())
    signed_headers_str = ";".join(signed_header_keys)
    canonical_headers = ""
    for k in signed_header_keys:
        canonical_headers += f"{k}:{headers_to_sign[k]}\n"

    canonical_request = f"{method}\n{canonical_uri}\n{canonical_querystring}\n"
    canonical_request += f"{canonical_headers}\n{signed_headers_str}\n{payload_hash}"

    credential_scope = f"{date_stamp}/cn-north-4/iotda/aws4_request"
    string_to_sign = f"SDK-HMAC-SHA256\n{amz_date}\n{credential_scope}\n"
    string_to_sign += hashlib.sha256(canonical_request.encode("utf-8")).hexdigest()

    signing_key = _get_signature_key(HUAWEI_SK, date_stamp, "cn-north-4", "iotda")
    signature = hmac.new(signing_key, string_to_sign.encode("utf-8"), hashlib.sha256).hexdigest()

    authorization = f"SDK-HMAC-SHA256 Credential={HUAWEI_AK}/{credential_scope}, "
    authorization += f"SignedHeaders={signed_headers_str}, Signature={signature}"

    req_headers = {
        "Host": IOTDA_ENDPOINT,
        "X-Sdk-Date": amz_date,
        "X-Sdk-Content-Sha256": payload_hash,
        "Authorization": authorization,
        "Content-Type": "application/json",
    }

    if IOTDA_INSTANCE_ID:
        req_headers["Instance-Id"] = IOTDA_INSTANCE_ID

    return req_headers


def get_device_shadow():
    """Query device shadow from IoTDA."""
    path = f"/v5/iot/{PROJECT_ID}/devices/{DEVICE_ID}/shadow"
    url = f"https://{IOTDA_ENDPOINT}{path}"

    headers = _build_headers("GET", path)
    resp = requests.get(url, headers=headers, timeout=10)

    if resp.status_code == 200:
        return resp.json()
    else:
        print(f"[IoTDA] Shadow query failed: {resp.status_code} {resp.text}")
        return None


def send_command(command_paras):
    """Send command to device via IoTDA.

    Args:
        command_paras: dict like {"led": "ON"} or {"buzzer": "FLASH"}
    """
    path = f"/v5/iot/{PROJECT_ID}/devices/{DEVICE_ID}/commands"
    url = f"https://{IOTDA_ENDPOINT}{path}"

    body = {
        "service_id": SERVICE_ID,
        "command_name": COMMAND_NAME,
        "paras": command_paras,
    }

    import json
    body_str = json.dumps(body)
    headers = _build_headers("POST", path, body_str)

    resp = requests.post(url, headers=headers, data=body_str, timeout=10)

    if resp.status_code in (200, 201):
        print(f"[IoTDA] Command sent: {body_str}")
        return True
    else:
        print(f"[IoTDA] Command failed: {resp.status_code} {resp.text}")
        return False
