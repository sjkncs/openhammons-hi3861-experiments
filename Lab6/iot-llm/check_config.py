"""
Lab6 Configuration Check Script

Checks:
1. All required config fields are non-empty
2. AK/SK can authenticate (basic connectivity test)
3. Device exists and shadow is queryable
"""

import sys
from config import (
    HUAWEI_AK, HUAWEI_SK, IOTDA_ENDPOINT, PROJECT_ID,
    DEVICE_ID, ZHIPU_API_KEY, SERVICE_ID, COMMAND_NAME
)

def check():
    errors = []
    warnings = []

    # 1. Check required fields
    fields = {
        "HUAWEI_AK": HUAWEI_AK,
        "HUAWEI_SK": HUAWEI_SK,
        "IOTDA_ENDPOINT": IOTDA_ENDPOINT,
        "PROJECT_ID": PROJECT_ID,
        "DEVICE_ID": DEVICE_ID,
        "ZHIPU_API_KEY": ZHIPU_API_KEY,
    }

    for name, value in fields.items():
        if not value or not value.strip():
            errors.append(f"[FAIL] {name} is empty")
        else:
            print(f"  [OK] {name} is set")

    print(f"\n  [OK] SERVICE_ID = {SERVICE_ID}")
    print(f"  [OK] COMMAND_NAME = {COMMAND_NAME}")

    if errors:
        print("\n=== ERRORS ===")
        for e in errors:
            print(f"  {e}")
        print("\nPlease fix config.py before proceeding.")
        return False

    # 2. Test IoTDA connectivity
    print("\n--- Testing IoTDA connectivity ---")
    try:
        from iotda_handler import get_device_shadow
        shadow = get_device_shadow()
        if shadow:
            print("  [OK] Device shadow retrieved successfully")
            # Check if shadow has data
            services = shadow.get("shadow", [])
            if services:
                print(f"  [OK] Shadow contains {len(services)} service(s)")
            else:
                warnings.append("Shadow is empty - device may not have reported data yet")
        else:
            errors.append("Failed to retrieve device shadow")
    except Exception as e:
        errors.append(f"IoTDA connectivity test failed: {e}")

    if warnings:
        print("\n=== WARNINGS ===")
        for w in warnings:
            print(f"  [WARN] {w}")

    if errors:
        print("\n=== ERRORS ===")
        for e in errors:
            print(f"  {e}")
        return False

    print("\n=== ALL CHECKS PASSED ===")
    return True

if __name__ == "__main__":
    print("Lab6 Configuration Check\n")
    ok = check()
    sys.exit(0 if ok else 1)
