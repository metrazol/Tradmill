import time

try:
    import requests
    _has_requests = True
except ImportError:
    _has_requests = False

from config import HA_WEBHOOK_URL, DEBUG

_last_post = 0


def init():
    if DEBUG:
        if HA_WEBHOOK_URL:
            print(f"[CONNECTIVITY] webhook {HA_WEBHOOK_URL}")
        else:
            print("[CONNECTIVITY] no webhook — offline")


def update(speed_mph, elapsed_sec, running):
    global _last_post
    if not HA_WEBHOOK_URL or not _has_requests:
        return
    now = time.monotonic()
    if now - _last_post < 5.0:
        return
    _last_post = now
    try:
        body = {"speed_mph": round(float(speed_mph), 2), "elapsed_sec": int(elapsed_sec), "running": bool(running)}
        resp = requests.post(HA_WEBHOOK_URL, json=body, timeout=1)
        if DEBUG:
            print(f"[CONNECTIVITY] HA webhook HTTP {resp.status_code}")
    except Exception as e:
        if DEBUG:
            print(f"[CONNECTIVITY] webhook failed: {e}")
