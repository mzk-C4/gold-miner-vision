/// GestureServer — Pure C++ standalone executable (x64)
/// Camera capture + hand tracking + HTTP web server + Web UI
/// Communicates with Win32 game via HTTP REST API on http://localhost:5000
///
/// Build: cmake -G "Visual Studio 18 2026" -A x64 .. && cmake --build . --config Release

#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <opencv2/opencv.hpp>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <sstream>
#include <cstdio>
#include <algorithm>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")

// ── Global State ──────────────────────────────────────────────────────

static std::mutex g_mutex;
static cv::Mat g_frame;
static float g_angle = 0.0f;
static std::string g_gesture = "NONE";
static bool g_connected = false;
static std::atomic<bool> g_running{true};

// ── Skin Detection ────────────────────────────────────────────────────

static bool detectSkin(const cv::Mat& frame, cv::Mat& mask) {
    cv::Mat hsv, ycrcb;
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);
    cv::cvtColor(frame, ycrcb, cv::COLOR_BGR2YCrCb);
    cv::Mat hsvMask, ycrcbMask;
    cv::inRange(hsv, cv::Scalar(0, 20, 70), cv::Scalar(25, 170, 255), hsvMask);
    cv::inRange(ycrcb, cv::Scalar(0, 135, 85), cv::Scalar(255, 180, 135), ycrcbMask);
    cv::bitwise_and(hsvMask, ycrcbMask, mask);
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::erode(mask, mask, kernel);
    cv::dilate(mask, mask, kernel);
    cv::dilate(mask, mask, kernel);
    cv::erode(mask, mask, kernel);
    return cv::countNonZero(mask) > 1000;
}

// ── Hand Landmark Approximation (MediaPipe-style 21-point model) ──────

struct Landmark { float x, y, z; };

static std::vector<Landmark> approximateLandmarks(const std::vector<cv::Point>& contour, const cv::Rect& bbox) {
    std::vector<Landmark> lm(21);
    float bw = (float)bbox.width, bh = (float)bbox.height;
    float bx = (float)bbox.x, by = (float)bbox.y;

    // Wrist (0): bottom-center of bounding box
    lm[0] = {bx + bw * 0.5f, by + bh, 0.0f};

    // Thumb: chain 2→3→4 curving left
    lm[2]  = {bx + bw * 0.15f, by + bh * 0.60f, 0.0f};
    lm[3]  = {bx + bw * 0.08f, by + bh * 0.42f, 0.0f};
    lm[4]  = {bx + bw * 0.02f, by + bh * 0.22f, 0.0f};

    // Index: 5→6→7→8
    lm[5]  = {bx + bw * 0.28f, by + bh * 0.45f, 0.0f};
    lm[6]  = {bx + bw * 0.26f, by + bh * 0.28f, 0.0f};
    lm[7]  = {bx + bw * 0.24f, by + bh * 0.12f, 0.0f};
    lm[8]  = {bx + bw * 0.22f, by + bh * 0.02f, 0.0f};

    // Middle: 9→10→11→12
    lm[9]  = {bx + bw * 0.44f, by + bh * 0.40f, 0.0f};
    lm[10] = {bx + bw * 0.44f, by + bh * 0.22f, 0.0f};
    lm[11] = {bx + bw * 0.44f, by + bh * 0.08f, 0.0f};
    lm[12] = {bx + bw * 0.44f, by + bh * 0.01f, 0.0f};

    // Ring: 13→14→15→16
    lm[13] = {bx + bw * 0.60f, by + bh * 0.42f, 0.0f};
    lm[14] = {bx + bw * 0.62f, by + bh * 0.25f, 0.0f};
    lm[15] = {bx + bw * 0.64f, by + bh * 0.10f, 0.0f};
    lm[16] = {bx + bw * 0.66f, by + bh * 0.02f, 0.0f};

    // Pinky: 17→18→19→20
    lm[17] = {bx + bw * 0.76f, by + bh * 0.48f, 0.0f};
    lm[18] = {bx + bw * 0.80f, by + bh * 0.32f, 0.0f};
    lm[19] = {bx + bw * 0.84f, by + bh * 0.16f, 0.0f};
    lm[20] = {bx + bw * 0.88f, by + bh * 0.04f, 0.0f};

    return lm;
}

static std::string classifyGestureByFingers(const std::vector<Landmark>& lm) {
    // Check finger extension: tip above PIP (lower Y = higher on screen = extended)
    int extended = 0;

    // Index: tip[8].y < pip[6].y
    if (lm[8].y < lm[6].y) extended++;
    // Middle: tip[12].y < pip[10].y
    if (lm[12].y < lm[10].y) extended++;
    // Ring: tip[16].y < pip[14].y
    if (lm[16].y < lm[14].y) extended++;
    // Pinky: tip[20].y < pip[18].y
    if (lm[20].y < lm[18].y) extended++;
    // Thumb: tip[4].x < ip[3].x (horizontal, thumb goes left when extended)
    if (lm[4].x < lm[3].x) extended++;

    if (extended >= 4) return "OPEN_PALM";
    if (extended == 0) return "FIST";
    return "NONE";
}

// ── Camera Thread ─────────────────────────────────────────────────────

static void cameraThread() {
    cv::VideoCapture cap(0, cv::CAP_DSHOW);
    if (!cap.isOpened()) {
        cap.open(1, cv::CAP_DSHOW);
    }
    if (!cap.isOpened()) {
        fprintf(stderr, "[GestureServer] Cannot open camera!\n");
        return;
    }
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    g_connected = true;
    printf("[GestureServer] Camera OK — hand tracking active\n");

    cv::Mat frame, skinMask;
    std::string lastGesture = "NONE";
    int stableCount = 0;

    while (g_running) {
        if (!cap.read(frame)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        cv::flip(frame, frame, 1);

        float angle = 0.0f;
        std::string gesture = "NONE";

        if (detectSkin(frame, skinMask)) {
            std::vector<std::vector<cv::Point>> contours;
            cv::findContours(skinMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

            double maxArea = 0;
            int bestIdx = -1;
            for (int i = 0; i < (int)contours.size(); i++) {
                double area = cv::contourArea(contours[i]);
                if (area > maxArea) { maxArea = area; bestIdx = i; }
            }

            if (bestIdx >= 0 && maxArea > 6000) {
                cv::Rect bbox = cv::boundingRect(contours[bestIdx]);

                // Angle from hand centroid
                cv::Moments m = cv::moments(contours[bestIdx]);
                if (m.m00 > 0) {
                    float cx = (float)(m.m10 / m.m00);
                    float normX = (cx - 64.0f) / (576.0f - 64.0f);
                    normX = (std::max)(0.0f, (std::min)(1.0f, normX));
                    angle = (normX - 0.5f) * 130.0f;
                }

                // Approximate landmarks and classify gesture
                auto lm = approximateLandmarks(contours[bestIdx], bbox);
                gesture = classifyGestureByFingers(lm);

                // Draw landmarks on frame
                for (size_t i = 0; i < lm.size(); i++) {
                    cv::circle(frame, cv::Point((int)lm[i].x, (int)lm[i].y),
                               i == 0 ? 6 : 3, cv::Scalar(0, 255, 255), -1);
                }

                // Draw contour
                cv::drawContours(frame, contours, bestIdx, cv::Scalar(0, 255, 0), 2);
            }
        }

        // Angle indicator
        cv::line(frame, cv::Point(320, 0), cv::Point(320, 480), cv::Scalar(255, 0, 0), 1);
        int angleX = 320 + (int)(angle * 3);
        cv::circle(frame, cv::Point(angleX, 240), 10, cv::Scalar(0, 0, 255), -1);

        // Stability filter
        if (gesture == lastGesture) stableCount++;
        else { stableCount = 0; lastGesture = gesture; }

        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_angle = angle;
            g_gesture = (stableCount >= 5) ? gesture : "NONE";
            g_frame = frame.clone();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    cap.release();
    g_connected = false;
}

// ── HTML Template ─────────────────────────────────────────────────────

static const char* HTML = R"(HTTP/1.1 200 OK
Content-Type: text/html; charset=utf-8
Connection: close

<!DOCTYPE html><html lang="zh"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>Gold Miner - Gesture Control</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#1a1a2e;font-family:Segoe UI,sans-serif;display:flex;flex-direction:column;align-items:center;min-height:100vh;color:#eee}
h1{margin:15px 0;color:#ffd700;font-size:26px}
#videoFeed{border:3px solid #ffd700;border-radius:12px;max-width:95vw;max-height:55vh}
.info-panel{display:flex;gap:30px;margin:15px 0;background:#16213e;padding:15px 30px;border-radius:10px}
.info-item{text-align:center}
.info-label{font-size:12px;color:#aaa;text-transform:uppercase;letter-spacing:1px}
.info-value{font-size:28px;font-weight:bold;margin-top:4px}
.g-open{color:#4caf50}.g-fist{color:#ff9800}.g-none{color:#888}
.angle-bar{width:320px;height:20px;background:#333;border-radius:10px;position:relative;overflow:hidden;margin:8px 0}
.angle-fill{height:100%;width:4px;background:#ffd700;position:absolute;left:50%;transition:left .08s}
.angle-labels{width:320px;display:flex;justify-content:space-between;font-size:11px;color:#666}
.dot{display:inline-block;width:10px;height:10px;border-radius:50%;margin-right:6px}
.dot-on{background:#4caf50}.dot-off{background:#ff5252}
</style></head><body>
<h1>Gold Miner - Gesture Control</h1>
<div><img id="videoFeed" src="/video_feed" alt="Camera"></div>
<div class="info-panel">
<div class="info-item"><div class="info-label">Gesture</div><div class="info-value g-none" id="gv">NONE</div></div>
<div class="info-item"><div class="info-label">Hook Angle</div><div class="info-value" id="av">0.0</div></div>
<div class="info-item"><div class="info-label">Status</div><div class="info-value" id="sv"><span class="dot dot-off"></span>OFF</div></div>
</div>
<div class="angle-bar"><div class="angle-fill" id="ab"></div></div>
<div class="angle-labels"><span>-65</span><span>0</span><span>+65</span></div>
<script>
setInterval(async()=>{try{const r=await fetch('/gesture'),d=await r.json();
document.getElementById('gv').textContent=d.gesture;
document.getElementById('gv').className='info-value g-'+(d.gesture==='OPEN_PALM'?'open':d.gesture==='FIST'?'fist':'none');
document.getElementById('av').textContent=d.angle.toFixed(1);
document.getElementById('sv').innerHTML='<span class="dot '+(d.connected?'dot-on':'dot-off')+'"></span>'+(d.connected?'ON':'OFF');
document.getElementById('ab').style.left=Math.min(100,Math.max(0,((d.angle+65)/130)*100))+'%'}catch(e){}},80)
</script></body></html>
)";

// ── HTTP Helpers ──────────────────────────────────────────────────────

static std::string makeJsonResponse(const std::string& body) {
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: application/json\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "Connection: close\r\n"
        << "Content-Length: " << body.size() << "\r\n\r\n"
        << body;
    return oss.str();
}

static std::string makeJpegResponse(const std::vector<uchar>& jpeg) {
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: image/jpeg\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "Connection: close\r\n"
        << "Content-Length: " << jpeg.size() << "\r\n\r\n";
    std::string header = oss.str();
    header.append(reinterpret_cast<const char*>(jpeg.data()), jpeg.size());
    return header;
}

static void sendAll(SOCKET s, const std::string& data) {
    send(s, data.c_str(), (int)data.size(), 0);
}

// ── HTTP Server ───────────────────────────────────────────────────────

static void handleClient(SOCKET client) {
    char buf[4096] = {};
    recv(client, buf, sizeof(buf) - 1, 0);
    std::string request(buf);

    // Parse first line: GET /path HTTP/1.1
    size_t p1 = request.find(' ');
    if (p1 == std::string::npos) { closesocket(client); return; }
    size_t p2 = request.find(' ', p1 + 1);
    if (p2 == std::string::npos) { closesocket(client); return; }
    std::string path = request.substr(p1 + 1, p2 - p1 - 1);

    if (path == "/" || path == "/index.html") {
        sendAll(client, HTML);
        closesocket(client);
        return;
    }

    if (path == "/gesture") {
        float angle;
        std::string gesture;
        bool connected;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            angle = g_angle;
            gesture = g_gesture;
            connected = g_connected;
        }
        char json[256];
        snprintf(json, sizeof(json),
                 "{\"angle\":%.1f,\"gesture\":\"%s\",\"connected\":%s}",
                 angle, gesture.c_str(), connected ? "true" : "false");
        sendAll(client, makeJsonResponse(json));
        closesocket(client);
        return;
    }

    if (path == "/frame") {
        std::vector<uchar> jpeg;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (!g_frame.empty())
                cv::imencode(".jpg", g_frame, jpeg, {cv::IMWRITE_JPEG_QUALITY, 40});
        }
        if (jpeg.empty()) {
            sendAll(client, "HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n");
        } else {
            sendAll(client, makeJpegResponse(jpeg));
        }
        closesocket(client);
        return;
    }

    if (path == "/video_feed") {
        // MJPEG stream — keep connection open
        std::string header =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: close\r\n\r\n";
        sendAll(client, header);

        while (g_running) {
            std::vector<uchar> jpeg;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                if (!g_frame.empty())
                    cv::imencode(".jpg", g_frame, jpeg, {cv::IMWRITE_JPEG_QUALITY, 55});
            }
            if (!jpeg.empty()) {
                std::ostringstream part;
                part << "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: "
                     << jpeg.size() << "\r\n\r\n";
                std::string p = part.str();
                p.append(reinterpret_cast<const char*>(jpeg.data()), jpeg.size());
                p += "\r\n";
                int ret = send(client, p.c_str(), (int)p.size(), 0);
                if (ret == SOCKET_ERROR) break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        closesocket(client);
        return;
    }

    if (path == "/shutdown") {
        g_running = false;
        sendAll(client, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nOK");
        closesocket(client);
        return;
    }

    // 404
    sendAll(client, "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    closesocket(client);
}

static void httpServer() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET server = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(5000);
    bind(server, (sockaddr*)&addr, sizeof(addr));
    listen(server, SOMAXCONN);

    printf("[GestureServer] HTTP server on http://localhost:5000\n");

    while (g_running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(server, &fds);
        timeval tv = {0, 100000}; // 100ms timeout
        if (select(0, &fds, nullptr, nullptr, &tv) > 0) {
            SOCKET client = accept(server, nullptr, nullptr);
            if (client != INVALID_SOCKET) {
                std::thread(handleClient, client).detach();
            }
        }
    }

    closesocket(server);
    WSACleanup();
}

// ── Main ──────────────────────────────────────────────────────────────

int main() {
    printf("==================================================\n");
    printf("  Gold Miner - Gesture Server (C++ Native)\n");
    printf("  Camera + Hand Tracking + HTTP Web UI\n");
    printf("  Web UI: http://localhost:5000\n");
    printf("  API:    http://localhost:5000/gesture\n");
    printf("==================================================\n");

    // Start camera thread
    std::thread cam(cameraThread);

    // Open browser after short delay
    std::thread([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        ShellExecuteA(nullptr, "open", "http://localhost:5000", nullptr, nullptr, SW_SHOW);
    }).detach();

    // Start HTTP server (blocking)
    httpServer();

    // Cleanup
    g_running = false;
    if (cam.joinable()) cam.join();
    printf("[GestureServer] Shutdown complete.\n");
    return 0;
}
