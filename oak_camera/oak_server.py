#!/usr/bin/env python3
"""
Oak D Lite Camera Server for AG Tech Environmental Monitor
Streams RGB + depth from Oak D Lite and serves a web dashboard.
Optionally pulls in DHT11 data from the ESP8266 dashboard.
"""

import sys
import time
import json
import threading
from http.server import HTTPServer, BaseHTTPRequestHandler
import cv2
import numpy as np
import depthai as dai

# ---------- Configuration ----------
HTTP_PORT = 8080
ESP_URL = None  # Set to "http://<esp-ip>/data" to pull DHT11 readings
PREVIEW_WIDTH = 640
PREVIEW_HEIGHT = 400
# -----------------------------------

latest_rgb = None
latest_depth_color = None
latest_stats = {"avg_depth_m": 0, "min_depth_m": 0, "max_depth_m": 0, "fps": 0}
frame_lock = threading.Lock()


def build_pipeline():
    """Create DepthAI pipeline: RGB camera + stereo depth."""
    pipeline = dai.Pipeline()

    # RGB camera
    cam_rgb = pipeline.create(dai.node.ColorCamera)
    cam_rgb.setPreviewSize(PREVIEW_WIDTH, PREVIEW_HEIGHT)
    cam_rgb.setInterleaved(False)
    cam_rgb.setFps(15)
    cam_rgb.setResolution(dai.ColorCameraProperties.SensorResolution.THE_1080_P)

    xout_rgb = pipeline.create(dai.node.XLinkOut)
    xout_rgb.setStreamName("rgb")
    cam_rgb.preview.link(xout_rgb.input)

    # Stereo depth
    mono_left = pipeline.create(dai.node.MonoCamera)
    mono_left.setResolution(dai.MonoCameraProperties.SensorResolution.THE_400_P)
    mono_left.setBoardSocket(dai.CameraBoardSocket.CAM_B)
    mono_left.setFps(15)

    mono_right = pipeline.create(dai.node.MonoCamera)
    mono_right.setResolution(dai.MonoCameraProperties.SensorResolution.THE_400_P)
    mono_right.setBoardSocket(dai.CameraBoardSocket.CAM_C)
    mono_right.setFps(15)

    stereo = pipeline.create(dai.node.StereoDepth)
    stereo.setDefaultProfilePreset(dai.node.StereoDepth.PresetMode.DEFAULT)
    stereo.setDepthAlign(dai.CameraBoardSocket.CAM_A)
    stereo.setOutputSize(PREVIEW_WIDTH, PREVIEW_HEIGHT)
    stereo.setLeftRightCheck(True)
    stereo.setSubpixel(True)

    mono_left.out.link(stereo.left)
    mono_right.out.link(stereo.right)

    xout_depth = pipeline.create(dai.node.XLinkOut)
    xout_depth.setStreamName("depth")
    stereo.depth.link(xout_depth.input)

    return pipeline


def camera_loop():
    """Run the DepthAI pipeline and update shared frame buffers."""
    global latest_rgb, latest_depth_color, latest_stats

    pipeline = build_pipeline()
    frame_count = 0
    fps_start = time.time()
    fps_val = 0

    with dai.Device(pipeline) as device:
        q_rgb = device.getOutputQueue("rgb", maxSize=4, blocking=False)
        q_depth = device.getOutputQueue("depth", maxSize=4, blocking=False)

        print(f"Oak D Lite streaming on http://localhost:{HTTP_PORT}")

        while True:
            in_rgb = q_rgb.tryGet()
            in_depth = q_depth.tryGet()

            if in_rgb is not None:
                frame_rgb = in_rgb.getCvFrame()
                frame_count += 1
                elapsed = time.time() - fps_start
                if elapsed >= 1.0:
                    fps_val = frame_count / elapsed
                    frame_count = 0
                    fps_start = time.time()

                with frame_lock:
                    _, buf = cv2.imencode('.jpg', frame_rgb, [cv2.IMWRITE_JPEG_QUALITY, 80])
                    latest_rgb = buf.tobytes()

            if in_depth is not None:
                depth_frame = in_depth.getFrame()  # uint16 mm
                valid = depth_frame[depth_frame > 0]

                stats = {
                    "fps": round(fps_val, 1),
                    "avg_depth_m": round(float(np.mean(valid)) / 1000, 2) if len(valid) else 0,
                    "min_depth_m": round(float(np.min(valid)) / 1000, 2) if len(valid) else 0,
                    "max_depth_m": round(float(np.max(valid)) / 1000, 2) if len(valid) else 0,
                }

                # Colorize depth
                depth_norm = cv2.normalize(depth_frame, None, 0, 255, cv2.NORM_MINMAX)
                depth_color = cv2.applyColorMap(depth_norm.astype(np.uint8), cv2.COLORMAP_JET)

                with frame_lock:
                    _, buf = cv2.imencode('.jpg', depth_color, [cv2.IMWRITE_JPEG_QUALITY, 80])
                    latest_depth_color = buf.tobytes()
                    latest_stats = stats

            time.sleep(0.001)


DASHBOARD_HTML = r"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>AG Tech - Oak D Lite</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#0f172a;color:#e2e8f0;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;padding:16px}
h1{text-align:center;font-size:1.4rem;margin-bottom:12px;color:#38bdf8}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:12px;max-width:1100px;margin:0 auto}
.card{background:#1e293b;border-radius:12px;padding:14px;border:1px solid #334155}
.card h2{font-size:.85rem;color:#94a3b8;margin-bottom:8px;text-transform:uppercase;letter-spacing:.5px}
.feed{width:100%;border-radius:8px;background:#000;aspect-ratio:16/10;object-fit:contain}
.stats{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:10px}
.stat{background:#0f172a;border-radius:8px;padding:10px;text-align:center}
.stat .val{font-size:1.5rem;font-weight:700;color:#38bdf8}
.stat .lbl{font-size:.7rem;color:#64748b;margin-top:2px}
.full{grid-column:1/-1}
.env-row{display:flex;gap:12px;justify-content:center;flex-wrap:wrap;margin-top:6px}
.env-card{background:#0f172a;border-radius:8px;padding:12px 24px;text-align:center;min-width:120px}
.env-card .val{font-size:1.8rem;font-weight:700}
.env-card .lbl{font-size:.7rem;color:#64748b}
.temp .val{color:#f97316}
.hum .val{color:#06b6d4}
.bar{height:6px;background:#334155;border-radius:3px;margin-top:6px;overflow:hidden}
.bar-fill{height:100%;border-radius:3px;transition:width .3s}
#status{text-align:center;font-size:.75rem;color:#22c55e;margin-top:8px}
</style>
</head>
<body>
<h1>AG Tech - Oak D Lite Camera</h1>
<div class="grid">
  <div class="card">
    <h2>RGB Feed</h2>
    <img class="feed" id="rgb" alt="RGB stream">
  </div>
  <div class="card">
    <h2>Depth Map</h2>
    <img class="feed" id="depth" alt="Depth stream">
  </div>
  <div class="card full">
    <h2>Camera Stats</h2>
    <div class="stats" id="cam-stats">
      <div class="stat"><div class="val" id="fps">--</div><div class="lbl">FPS</div></div>
      <div class="stat"><div class="val" id="avg-d">--</div><div class="lbl">Avg Depth (m)</div></div>
      <div class="stat"><div class="val" id="min-d">--</div><div class="lbl">Min Depth (m)</div></div>
      <div class="stat"><div class="val" id="max-d">--</div><div class="lbl">Max Depth (m)</div></div>
    </div>
  </div>
  <div class="card full" id="env-section" style="display:none">
    <h2>Environment (ESP8266)</h2>
    <div class="env-row">
      <div class="env-card temp"><div class="val" id="temp">--</div><div class="lbl">Temperature C</div>
        <div class="bar"><div class="bar-fill" id="temp-bar" style="width:0%;background:#f97316"></div></div>
      </div>
      <div class="env-card hum"><div class="val" id="hum">--</div><div class="lbl">Humidity %</div>
        <div class="bar"><div class="bar-fill" id="hum-bar" style="width:0%;background:#06b6d4"></div></div>
      </div>
    </div>
  </div>
</div>
<div id="status">Connecting...</div>
<script>
const rgb=document.getElementById('rgb'), depth=document.getElementById('depth');
let seq=0;
function refreshFeeds(){
  const t=Date.now();
  rgb.src='/feed/rgb?t='+t;
  depth.src='/feed/depth?t='+t;
}
function poll(){
  fetch('/api/stats').then(r=>r.json()).then(d=>{
    document.getElementById('fps').textContent=d.fps;
    document.getElementById('avg-d').textContent=d.avg_depth_m;
    document.getElementById('min-d').textContent=d.min_depth_m;
    document.getElementById('max-d').textContent=d.max_depth_m;
    document.getElementById('status').textContent='Connected - '+new Date().toLocaleTimeString();
    if(d.temp!==undefined){
      document.getElementById('env-section').style.display='';
      document.getElementById('temp').textContent=d.temp.toFixed(1);
      document.getElementById('hum').textContent=d.hum.toFixed(1);
      document.getElementById('temp-bar').style.width=Math.min(d.temp/50*100,100)+'%';
      document.getElementById('hum-bar').style.width=d.hum+'%';
    }
  }).catch(()=>{document.getElementById('status').textContent='Disconnected';});
}
setInterval(refreshFeeds, 200);
setInterval(poll, 1000);
refreshFeeds(); poll();
</script>
</body>
</html>"""


class Handler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        pass  # quiet

    def do_GET(self):
        if self.path == '/' or self.path == '/index.html':
            self.send_response(200)
            self.send_header('Content-Type', 'text/html')
            self.end_headers()
            self.wfile.write(DASHBOARD_HTML.encode())

        elif self.path.startswith('/feed/rgb'):
            with frame_lock:
                data = latest_rgb
            if data:
                self.send_response(200)
                self.send_header('Content-Type', 'image/jpeg')
                self.send_header('Cache-Control', 'no-store')
                self.end_headers()
                self.wfile.write(data)
            else:
                self.send_error(503)

        elif self.path.startswith('/feed/depth'):
            with frame_lock:
                data = latest_depth_color
            if data:
                self.send_response(200)
                self.send_header('Content-Type', 'image/jpeg')
                self.send_header('Cache-Control', 'no-store')
                self.end_headers()
                self.wfile.write(data)
            else:
                self.send_error(503)

        elif self.path.startswith('/api/stats'):
            with frame_lock:
                stats = dict(latest_stats)
            # Optionally pull ESP8266 env data
            if ESP_URL:
                try:
                    import urllib.request
                    with urllib.request.urlopen(ESP_URL, timeout=1) as resp:
                        esp = json.loads(resp.read())
                        stats['temp'] = esp.get('temp', 0)
                        stats['hum'] = esp.get('hum', 0)
                except Exception:
                    pass
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Cache-Control', 'no-store')
            self.end_headers()
            self.wfile.write(json.dumps(stats).encode())

        else:
            self.send_error(404)


def main():
    global ESP_URL

    # Optional: pass ESP8266 IP as argument
    if len(sys.argv) > 1:
        ESP_URL = f"http://{sys.argv[1]}/data"
        print(f"Will pull ESP8266 data from {ESP_URL}")

    # Start camera in background thread
    cam_thread = threading.Thread(target=camera_loop, daemon=True)
    cam_thread.start()

    # Give camera a moment to start
    time.sleep(2)

    server = HTTPServer(('0.0.0.0', HTTP_PORT), Handler)
    print(f"Dashboard: http://localhost:{HTTP_PORT}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopped.")
        server.server_close()


if __name__ == '__main__':
    main()
