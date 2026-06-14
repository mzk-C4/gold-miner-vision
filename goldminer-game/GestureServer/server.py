"""
GestureServer — Flask + MediaPipe Hand Tracking Web Server.
Based on the community-standard HandTrackingModule pattern.
Serves web UI for camera-based gesture recognition.
Communicates with C++ game via HTTP REST API.

Usage: python server.py
Web UI: http://localhost:5000
API:
  GET /gesture       -> {"angle": float, "gesture": "OPEN_PALM"|"FIST"|"NONE", "connected": bool}
  GET /frame         -> JPEG snapshot
  GET /video_feed    -> MJPEG live stream
"""

import cv2
import mediapipe as mp
import time
import math
import threading
from flask import Flask, Response, render_template_string, jsonify

app = Flask(__name__)

# ── HandDetector (community-standard pattern) ──────────────────────────

class HandDetector:
    def __init__(self, mode=False, maxHands=1, detectionCon=0.7, trackCon=0.7):
        self.mode = mode
        self.maxHands = maxHands
        self.detectionCon = detectionCon
        self.trackCon = trackCon

        self.mpHands = mp.solutions.hands
        self.hands = self.mpHands.Hands(
            static_image_mode=self.mode,
            max_num_hands=self.maxHands,
            min_detection_confidence=self.detectionCon,
            min_tracking_confidence=self.trackCon
        )
        self.mpDraw = mp.solutions.drawing_utils
        self.drawStyles = mp.solutions.drawing_styles
        self.tipIds = [4, 8, 12, 16, 20]  # 指尖 landmark ID
        self.lmList = []
        self.results = None

    def findHands(self, img, draw=True):
        imgRGB = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
        self.results = self.hands.process(imgRGB)

        if self.results.multi_hand_landmarks:
            for handLms in self.results.multi_hand_landmarks:
                if draw:
                    self.mpDraw.draw_landmarks(
                        img, handLms, self.mpHands.HAND_CONNECTIONS,
                        self.drawStyles.get_default_hand_landmarks_style(),
                        self.drawStyles.get_default_hand_connections_style()
                    )
        return img

    def findPosition(self, img, handNo=0, draw=True):
        xList, yList = [], []
        self.lmList = []
        bbox = []

        if self.results.multi_hand_landmarks:
            if handNo >= len(self.results.multi_hand_landmarks):
                return [], bbox
            myHand = self.results.multi_hand_landmarks[handNo]
            for id, lm in enumerate(myHand.landmark):
                h, w, c = img.shape
                cx, cy = int(lm.x * w), int(lm.y * h)
                xList.append(cx)
                yList.append(cy)
                self.lmList.append([id, cx, cy])
                if draw:
                    cv2.circle(img, (cx, cy), 5, (255, 0, 255), cv2.FILLED)

            xmin, xmax = min(xList), max(xList)
            ymin, ymax = min(yList), max(yList)
            bbox = xmin, ymin, xmax - xmin, ymax - ymin

            if draw:
                cv2.rectangle(img, (bbox[0] - 20, bbox[1] - 20),
                              (bbox[0] + bbox[2] + 20, bbox[1] + bbox[3] + 20),
                              (0, 255, 0), 2)
        return self.lmList, bbox

    def fingersUp(self):
        """Return [thumb, index, middle, ring, pinky] — 1 = up, 0 = down."""
        fingers = []
        if len(self.lmList) == 0:
            return [0, 0, 0, 0, 0]

        # Thumb: compare tip X with IP joint X (horizontal)
        if self.lmList[self.tipIds[0]][1] > self.lmList[self.tipIds[0] - 1][1]:
            fingers.append(1)
        else:
            fingers.append(0)

        # Other 4 fingers: compare tip Y with PIP joint Y (vertical)
        for id in range(1, 5):
            if self.lmList[self.tipIds[id]][2] < self.lmList[self.tipIds[id] - 2][2]:
                fingers.append(1)
            else:
                fingers.append(0)
        return fingers

    def findDistance(self, p1, p2, img, draw=True):
        x1, y1 = self.lmList[p1][1], self.lmList[p1][2]
        x2, y2 = self.lmList[p2][1], self.lmList[p2][2]
        cx, cy = (x1 + x2) // 2, (y1 + y2) // 2

        if draw:
            cv2.circle(img, (x1, y1), 10, (255, 0, 255), cv2.FILLED)
            cv2.circle(img, (x2, y2), 10, (255, 0, 255), cv2.FILLED)
            cv2.line(img, (x1, y1), (x2, y2), (255, 0, 255), 3)
            cv2.circle(img, (cx, cy), 10, (255, 0, 255), cv2.FILLED)

        length = math.hypot(x2 - x1, y2 - y1)
        return length, img, [x1, y1, x2, y2, cx, cy]

    def getAngle(self, img):
        """Map wrist X position to hook angle (-65..+65 degrees)."""
        if len(self.lmList) == 0:
            return 0.0
        wrist_x = self.lmList[0][1]  # landmark 0 = wrist
        w = img.shape[1]
        norm_x = (wrist_x - 64) / (w - 128)  # edge margin
        norm_x = max(0.0, min(1.0, norm_x))
        return (norm_x - 0.5) * 130.0

    def classifyGesture(self):
        """OPEN_PALM = 4+ fingers up, FIST = 0 fingers up, else NONE."""
        fingers = self.fingersUp()
        n = fingers.count(1)
        if n >= 4:
            return "OPEN_PALM"
        elif n == 0:
            return "FIST"
        return "NONE"


# ── Global State ───────────────────────────────────────────────────────

g_detector = HandDetector(maxHands=1, detectionCon=0.7, trackCon=0.7)
g_frame = None
g_frame_lock = threading.Lock()
g_angle = 0.0
g_gesture = "NONE"
g_connected = False
g_running = True

# ── Web UI Template ────────────────────────────────────────────────────

HTML_TEMPLATE = """<!DOCTYPE html>
<html lang="zh">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Gold Miner - Gesture Control</title>
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body {
    background: #1a1a2e;
    font-family: 'Segoe UI', sans-serif;
    display: flex; flex-direction: column; align-items: center;
    min-height: 100vh; color: #eee;
  }
  h1 { margin: 15px 0; color: #ffd700; font-size: 26px; }
  .container { position: relative; display: inline-block; }
  #videoFeed {
    border: 3px solid #ffd700; border-radius: 12px;
    max-width: 95vw; max-height: 55vh;
  }
  .info-panel {
    display: flex; gap: 30px; margin: 15px 0;
    background: #16213e; padding: 15px 30px; border-radius: 10px;
  }
  .info-item { text-align: center; }
  .info-label { font-size: 12px; color: #aaa; text-transform: uppercase; letter-spacing: 1px; }
  .info-value { font-size: 28px; font-weight: bold; margin-top: 4px; }
  .g-open { color: #4caf50; }
  .g-fist { color: #ff9800; }
  .g-none { color: #888; }
  .angle-bar {
    width: 320px; height: 20px; background: #333;
    border-radius: 10px; position: relative; overflow: hidden; margin: 8px 0;
  }
  .angle-fill {
    height: 100%; width: 4px; background: #ffd700;
    position: absolute; left: 50%; transition: left 0.08s;
  }
  .angle-labels { width: 320px; display: flex; justify-content: space-between; font-size: 11px; color: #666; }
  .dot { display: inline-block; width: 10px; height: 10px; border-radius: 50%; margin-right: 6px; }
  .dot-on { background: #4caf50; }
  .dot-off { background: #ff5252; }
</style>
</head>
<body>
  <h1>Gold Miner - Gesture Control</h1>
  <div class="container">
    <img id="videoFeed" src="/video_feed" alt="Camera">
  </div>

  <div class="info-panel">
    <div class="info-item">
      <div class="info-label">Gesture</div>
      <div class="info-value g-none" id="gestureVal">NONE</div>
    </div>
    <div class="info-item">
      <div class="info-label">Hook Angle</div>
      <div class="info-value" id="angleVal">0.0</div>
    </div>
    <div class="info-item">
      <div class="info-label">Status</div>
      <div class="info-value" id="statusVal">
        <span class="dot dot-off"></span>OFF
      </div>
    </div>
  </div>

  <div class="angle-bar"><div class="angle-fill" id="angleBar"></div></div>
  <div class="angle-labels"><span>-65</span><span>0</span><span>+65</span></div>

  <script>
    setInterval(async () => {
      try {
        const resp = await fetch('/gesture');
        const data = await resp.json();
        const gv = document.getElementById('gestureVal');
        gv.textContent = data.gesture;
        gv.className = 'info-value g-' +
          (data.gesture === 'OPEN_PALM' ? 'open' : data.gesture === 'FIST' ? 'fist' : 'none');

        document.getElementById('angleVal').textContent = data.angle.toFixed(1);

        const dot = document.querySelector('.dot');
        dot.className = 'dot ' + (data.connected ? 'dot-on' : 'dot-off');
        document.getElementById('statusVal').innerHTML =
          '<span class="dot ' + (data.connected ? 'dot-on' : 'dot-off') + '"></span>' +
          (data.connected ? 'ON' : 'OFF');

        const pct = ((data.angle + 65) / 130) * 100;
        document.getElementById('angleBar').style.left = Math.min(100, Math.max(0, pct)) + '%';
      } catch(e) {}
    }, 80);
  </script>
</body>
</html>"""


# ── Camera Thread ──────────────────────────────────────────────────────

def camera_thread():
    global g_frame, g_angle, g_gesture, g_connected, g_running

    cap = cv2.VideoCapture(0)
    if not cap.isOpened():
        cap = cv2.VideoCapture(1)
    if not cap.isOpened():
        print("[GestureServer] ERROR: No camera!")
        return

    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    g_connected = True
    print("[GestureServer] Camera OK — MediaPipe hand tracking active")

    last_gesture = "NONE"
    stable_count = 0
    pTime = 0

    while g_running:
        success, img = cap.read()
        if not success:
            time.sleep(0.01)
            continue

        img = cv2.flip(img, 1)
        img = g_detector.findHands(img, draw=True)
        lmList, bbox = g_detector.findPosition(img, handNo=0, draw=False)

        if len(lmList) != 0:
            angle = g_detector.getAngle(img)
            gesture = g_detector.classifyGesture()

            # Draw angle indicator
            cv2.line(img, (320, 0), (320, 480), (255, 0, 0), 1)
            angle_x = 320 + int(angle * 3)
            cv2.circle(img, (angle_x, 240), 10, (0, 0, 255), -1)
        else:
            angle = 0.0
            gesture = "NONE"

        # Stability filter
        if gesture == last_gesture:
            stable_count += 1
        else:
            stable_count = 0
            last_gesture = gesture

        g_angle = angle
        g_gesture = gesture if stable_count >= 4 else "NONE"

        cTime = time.time()
        fps = 1.0 / (cTime - pTime + 0.001)
        pTime = cTime
        cv2.putText(img, f"FPS:{int(fps)}", (10, 50),
                    cv2.FONT_HERSHEY_PLAIN, 2, (255, 0, 255), 2)

        with g_frame_lock:
            g_frame = img.copy()

        time.sleep(0.01)

    cap.release()
    g_connected = False


# ── Flask Routes ───────────────────────────────────────────────────────

def generate_frames():
    while True:
        with g_frame_lock:
            if g_frame is None:
                time.sleep(0.03)
                continue
            _, jpeg = cv2.imencode('.jpg', g_frame, [cv2.IMWRITE_JPEG_QUALITY, 55])
        yield (b'--frame\r\nContent-Type: image/jpeg\r\n\r\n'
               + jpeg.tobytes() + b'\r\n')
        time.sleep(0.05)


@app.route('/')
def index():
    return render_template_string(HTML_TEMPLATE)


@app.route('/video_feed')
def video_feed():
    return Response(generate_frames(),
                    mimetype='multipart/x-mixed-replace; boundary=frame')


@app.route('/gesture')
def gesture_api():
    return jsonify({
        'angle': round(g_angle, 1),
        'gesture': g_gesture,
        'connected': g_connected
    })


@app.route('/frame')
def frame_api():
    with g_frame_lock:
        if g_frame is None:
            return '', 204
        _, jpeg = cv2.imencode('.jpg', g_frame, [cv2.IMWRITE_JPEG_QUALITY, 40])
    return Response(jpeg.tobytes(), mimetype='image/jpeg')


@app.route('/shutdown')
def shutdown():
    global g_running
    g_running = False
    return 'OK'


# ── Main ───────────────────────────────────────────────────────────────

if __name__ == '__main__':
    print("=" * 50)
    print("  Gold Miner - Gesture Server")
    print("  MediaPipe Hand Tracking + Flask Web UI")
    print("  Web: http://localhost:5000")
    print("  API: http://localhost:5000/gesture")
    print("=" * 50)

    # Camera background thread
    cam = threading.Thread(target=camera_thread, daemon=True)
    cam.start()

    # Open browser
    def open_browser():
        time.sleep(1.5)
        import webbrowser
        webbrowser.open('http://localhost:5000')

    threading.Thread(target=open_browser, daemon=True).start()

    try:
        app.run(host='0.0.0.0', port=5000, debug=False, use_reloader=False)
    finally:
        g_running = False
        cam.join(timeout=2)
        print("[GestureServer] Bye.")
