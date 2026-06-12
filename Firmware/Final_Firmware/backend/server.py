"""
bike_backend/server.py
FastAPI WebSocket bridge for the ESP32 bike computer.

Architecture:
  - ESP32 connects to  ws://HOST:8000/ws/device
  - Browser connects to ws://HOST:8000/ws/dashboard
  - REST GET  /config          → returns current config (wheel circumference etc.)
  - REST POST /config          → update config; pushed to ESP32 on next config poll
  - REST GET  /history         → last N speed samples as JSON
  - Static files served from  ./static/  (the dashboard HTML)

Run:
  pip install fastapi uvicorn websockets
  uvicorn server:app --host 0.0.0.0 --port 8000 --reload
"""

import asyncio
import json
import time
from collections import deque
from typing import Optional

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse, JSONResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel

app = FastAPI(title="Bike Computer Backend")

# ── Shared state ──────────────────────────────────────────────
MAX_HISTORY = 300  # samples kept in memory (~5 min at 1 Hz)

state: dict = {
    "speed_mph": 0.0,
    "squeak_detected": False,
    "squeak_confidence": 0.0,
    "normal_confidence": 0.0,
    "gps_valid": False,
    "gps_hours": 0,
    "gps_minutes": 0,
    "gps_seconds": 0,
    "gps_latitude": 0.0,
    "gps_longitude": 0.0,
    "gps_satellites": 0,
    "delta_us": 0,
    "ts": 0,
}

config: dict = {
    "wheel_circumference_m": 2.18,  # default 700x35C
}

history: deque = deque(maxlen=MAX_HISTORY)  # list of {ts, speed_mph}

# ── Connection managers ───────────────────────────────────────
device_ws: Optional[WebSocket] = None
dashboard_clients: list[WebSocket] = []


async def broadcast_to_dashboards(payload: dict):
    dead = []
    for ws in dashboard_clients:
        try:
            await ws.send_json(payload)
        except Exception:
            dead.append(ws)
    for ws in dead:
        dashboard_clients.remove(ws)


# ── Device WebSocket  (ESP32 connects here) ───────────────────
@app.websocket("/ws/device")
async def device_endpoint(ws: WebSocket):
    global device_ws
    print("[DEVICE] incoming websocket")
    await ws.accept()
    print("[DEVICE] websocket accepted")
    device_ws = ws
    print("[DEVICE] ESP32 connected")
    try:
        while True:
            raw = await ws.receive_text()
            print(f"[DEVICE] RX: {raw}")
            try:
                msg = json.loads(raw)
            except json.JSONDecodeError:
                continue

            ts = time.time()
            msg["ts"] = ts

            # Update live state
            state.update(msg)
            state["ts"] = ts

            # Record history for speed chart
            history.append({"ts": ts, "speed_mph": msg.get("speed_mph", 0.0)})

            # Forward to all browser clients
            await broadcast_to_dashboards({"type": "telemetry", **msg})

    except WebSocketDisconnect:
        print("[DEVICE] ESP32 disconnected")
        device_ws = None


# ── Dashboard WebSocket  (browser connects here) ──────────────
@app.websocket("/ws/dashboard")
async def dashboard_endpoint(ws: WebSocket):
    await ws.accept()
    dashboard_clients.append(ws)
    print(f"[DASH] Browser connected ({len(dashboard_clients)} total)")

    # Send current state immediately so the page isn't blank on load
    await ws.send_json({"type": "init", "state": state, "config": config})
    try:
        while True:
            # browsers can send config updates via this socket too
            raw = await ws.receive_text()
            try:
                msg = json.loads(raw)
            except json.JSONDecodeError:
                continue
            if msg.get("type") == "set_config":
                config.update({k: v for k, v in msg.items() if k != "type"})
                print(f"[CONFIG] Updated via dashboard WS: {config}")
                # Echo config to all dashboards so every tab stays in sync
                await broadcast_to_dashboards({"type": "config", **config})
                # Push to ESP32 if connected
                if device_ws:
                    try:
                        await device_ws.send_json({"type": "config", **config})
                    except Exception as e:
                        print(f"[CONFIG] Failed to push to device: {e}")
    except WebSocketDisconnect:
        dashboard_clients.remove(ws)
        print(f"[DASH] Browser disconnected ({len(dashboard_clients)} remaining)")


# ── REST endpoints ────────────────────────────────────────────
class ConfigUpdate(BaseModel):
    wheel_circumference_m: Optional[float] = None


@app.get("/config")
def get_config():
    """ESP32 polls this on boot and after reconnect."""
    return JSONResponse(config)


@app.post("/config")
async def post_config(update: ConfigUpdate):
    """Update config from a REST client (curl, Postman, etc.)."""
    if update.wheel_circumference_m is not None:
        config["wheel_circumference_m"] = update.wheel_circumference_m
    await broadcast_to_dashboards({"type": "config", **config})
    if device_ws:
        try:
            await device_ws.send_json({"type": "config", **config})
        except Exception as e:
            print(f"[CONFIG] REST push to device failed: {e}")
    return JSONResponse({"ok": True, "config": config})


@app.get("/state")
def get_state():
    return JSONResponse(state)


@app.get("/history")
def get_history():
    return JSONResponse(list(history))


# ── Static files (dashboard HTML) ────────────────────────────
app.mount("/static", StaticFiles(directory="static"), name="static")


@app.get("/")
def root():
    return FileResponse("static/index.html")
