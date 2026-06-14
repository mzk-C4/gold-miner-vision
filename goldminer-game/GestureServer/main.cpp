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
static std::vector<unsigned char> g_jpegBytes;  // pre-encoded JPEG for /frame endpoint
static float g_angle = 0.0f;
static std::string g_gesture = "NONE";
static bool g_connected = false;
static std::atomic<bool> g_running{true};

// ── Skin Detection (HSV + YCrCb, with dynamic lighting adaptation) ────
// 策略4：动态亮度自适应
//   - 实时计算帧平均亮度
//   - 暗光环境：扩展 HSV 的 V 下界，收紧 YCrCb 以排除噪声
//   - 亮光环境：收紧 HSV 以排除过曝区域

static void detectSkin(const cv::Mat& frame, cv::Mat& mask) {
    // 计算平均亮度用于自适应阈值
    cv::Scalar meanBGR = cv::mean(frame);
    double brightness = 0.299 * meanBGR[2] + 0.587 * meanBGR[1] + 0.114 * meanBGR[0];  // perceived

    cv::Mat hsv, ycrcb;
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);
    cv::cvtColor(frame, ycrcb, cv::COLOR_BGR2YCrCb);

    // 动态 HSV 阈值（基于亮度）
    int vLow, vHigh;
    if (brightness < 60) {
        // 极暗：大幅放宽 V 下界，依赖 YCrCb 排除噪声
        vLow = 30; vHigh = 255;
    } else if (brightness < 120) {
        // 偏暗
        vLow = 50; vHigh = 255;
    } else if (brightness > 200) {
        // 过亮：收紧 V 下界排除过曝
        vLow = 90; vHigh = 255;
    } else {
        // 正常光照
        vLow = 70; vHigh = 255;
    }

    cv::Mat hsvMask;
    cv::inRange(hsv, cv::Scalar(0, 20, vLow), cv::Scalar(25, 170, vHigh), hsvMask);

    cv::Mat ycrcbMask;
    cv::inRange(ycrcb, cv::Scalar(0, 60, 77), cv::Scalar(255, 173, 127), ycrcbMask);

    cv::bitwise_and(hsvMask, ycrcbMask, mask);

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::erode(mask, mask, kernel);
    cv::dilate(mask, mask, kernel);
    cv::GaussianBlur(mask, mask, cv::Size(3, 3), 0);
}

// ── Palm Circle Classification (Reference: OpenCVHandGuesture) ──────────
// 完全参照 vision/OpenCVHandGuesture/main.cpp 的掌心圆算法
//
// 核心原理（用户指出）：
//   - 通过凸包缺陷点拟合"掌心圆"
//   - 握拳时：缺陷点挤在一起 → 圆半径很小 → FIST
//   - 张手时：缺陷点在指缝位置 → 圆半径很大 → OPEN_PALM
//   - 缺陷不足 2 个 → 轮廓紧凑 → FIST
//
// 这比指尖距离法更稳定：不依赖指尖检测，只需要凸包缺陷 + 三点拟合圆

static double dist2(const cv::Point& a, const cv::Point& b) {
    double dx = a.x - b.x, dy = a.y - b.y;
    return dx * dx + dy * dy;
}

// 三点拟合圆（返回圆心和半径）
static std::pair<cv::Point, double> circleFromPoints(cv::Point p1, cv::Point p2, cv::Point p3) {
    double offset = std::pow(p2.x, 2.0) + std::pow(p2.y, 2.0);
    double bc = (std::pow(p1.x, 2.0) + std::pow(p1.y, 2.0) - offset) / 2.0;
    double cd = (offset - std::pow(p3.x, 2.0) - std::pow(p3.y, 2.0)) / 2.0;
    double det = (p1.x - p2.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p2.y);
    if (std::abs(det) < 0.0000001) return {cv::Point(0, 0), 0.0};
    double idet = 1.0 / det;
    double cx = (bc * (p2.y - p3.y) - cd * (p1.y - p2.y)) * idet;
    double cy = (cd * (p1.x - p2.x) - bc * (p2.x - p3.x)) * idet;
    double radius = std::sqrt(std::pow(p2.x - cx, 2.0) + std::pow(p2.y - cy, 2.0));
    return {cv::Point((int)cx, (int)cy), radius};
}

// 时域平滑：对掌心圆做移动平均（参照 reference 10 帧平均）
static std::vector<std::pair<cv::Point, double>> g_palmHistory;  // 掌心圆历史
static const int PALM_HISTORY_MAX = 4;

static std::string classifyByPalmCircle(const std::vector<cv::Point>& contour,
                                        cv::Point& outPalm, double& outRadius,
                                        const std::string& prevGesture,
                                        cv::Mat* debugFrame) {
    outPalm = cv::Point(0, 0);
    outRadius = 0;
    if (contour.size() < 20) return "NONE";

    double area = cv::contourArea(contour);
    if (area < 2000) return "NONE";

    // 1. 凸包
    std::vector<int> hullIdx;
    cv::convexHull(contour, hullIdx, false, false);
    if (hullIdx.size() < 3) return "NONE";

    // 2. 凸包缺陷（hullIdx 必须单调递增，convexityDefects 硬性要求）
    std::sort(hullIdx.begin(), hullIdx.end());
    std::vector<cv::Vec4i> defects;
    cv::convexityDefects(contour, hullIdx, defects);

    // 3. 缺陷不足 2 个 → 轮廓太紧凑 → 握拳
    if (defects.size() < 2) {
        // 即使判 FIST，也算一下等效半径用于调试
        cv::Moments m = cv::moments(contour);
        if (m.m00 > 0) {
            outPalm = cv::Point((int)(m.m10 / m.m00), (int)(m.m01 / m.m00));
            outRadius = std::sqrt(area / CV_PI);
        }
        if (debugFrame) {
            cv::circle(*debugFrame, outPalm, (int)outRadius, cv::Scalar(0, 165, 255), 1);
            cv::putText(*debugFrame, "FIST (def<2)", cv::Point(10, 50),
                        cv::FONT_HERSHEY_COMPLEX_SMALL, 1.0, cv::Scalar(0, 165, 255), 1);
        }
        return "FIST";
    }

    // 4. 参照 reference：收集所有缺陷点，计算粗略掌心
    std::vector<cv::Point> palmPts;
    cv::Point rough(0, 0);
    int n = 0;
    for (const auto& d : defects) {
        palmPts.push_back(contour[d[0]]);  // start
        palmPts.push_back(contour[d[1]]);  // end
        palmPts.push_back(contour[d[2]]);  // far
        rough += contour[d[0]] + contour[d[1]] + contour[d[2]];
        n += 3;
    }
    rough.x /= n; rough.y /= n;

    // 5. 找距粗略掌心最近的 3 个缺陷点 → 拟合掌心圆
    std::vector<std::pair<double, int>> dv;
    for (int i = 0; i < (int)palmPts.size(); i++)
        dv.push_back({dist2(rough, palmPts[i]), i});
    std::sort(dv.begin(), dv.end());

    cv::Point palm = rough;
    double defectCircleRadius = 10.0;
    for (int i = 0; i + 2 < (int)dv.size(); i++) {
        auto c = circleFromPoints(palmPts[dv[i].second],
                                  palmPts[dv[i+1].second],
                                  palmPts[dv[i+2].second]);
        if (c.second > 0) { palm = c.first; defectCircleRadius = c.second; break; }
    }

    // 6. 时域平滑（参照 reference 的 palm_centers 历史窗口）
    g_palmHistory.push_back({palm, defectCircleRadius});
    if ((int)g_palmHistory.size() > PALM_HISTORY_MAX)
        g_palmHistory.erase(g_palmHistory.begin());

    cv::Point avgPalm(0, 0);
    double avgDefectRadius = 0;
    for (const auto& h : g_palmHistory) {
        avgPalm += h.first;
        avgDefectRadius += h.second;
    }
    avgPalm.x /= (int)g_palmHistory.size();
    avgPalm.y /= (int)g_palmHistory.size();
    avgDefectRadius /= (int)g_palmHistory.size();

    outPalm = avgPalm;
    outRadius = avgDefectRadius;

    // 7. 核心判定：掌心圆半径 vs 轮廓等效半径
    //    握拳 → 缺陷点挤在一起 → 圆很小 (< 0.45 * contourRadius)
    //    张手 → 缺陷点在指缝 → 圆很大 (>= 0.45 * contourRadius)
    double contourRadius = std::sqrt(area / CV_PI);
    double circleRatio = contourRadius > 0 ? avgDefectRadius / contourRadius : 0;

    std::string result;
    if (avgDefectRadius < 35.0 || circleRatio < 0.35)
        result = "FIST";
    else if (circleRatio >= 0.35)
        result = "OPEN_PALM";
    else
        // 滞回：中间状态保持上一帧
        result = prevGesture.empty() || prevGesture == "NONE" ? "FIST" : prevGesture;

    // 8. Debug 绘制
    if (debugFrame) {
        // 掌心圆
        cv::circle(*debugFrame, avgPalm, (int)avgDefectRadius, cv::Scalar(144, 144, 255), 2);
        cv::circle(*debugFrame, avgPalm, 5, cv::Scalar(144, 144, 255), -1);
        // 轮廓等效圆（虚线参考）
        cv::circle(*debugFrame, avgPalm, (int)contourRadius, cv::Scalar(100, 100, 100), 1);
        // 缺陷点
        for (const auto& d : defects)
            cv::circle(*debugFrame, contour[d[2]], 2, cv::Scalar(0, 0, 255), -1);

        char buf[80];
        snprintf(buf, sizeof(buf), "%s defR=%.0f cntR=%.0f ratio=%.2f",
                 result.c_str(), avgDefectRadius, contourRadius, circleRatio);
        cv::putText(*debugFrame, buf, cv::Point(10, 50),
                    cv::FONT_HERSHEY_COMPLEX_SMALL, 1.0,
                    result == "OPEN_PALM" ? cv::Scalar(0, 255, 0) :
                    result == "FIST" ? cv::Scalar(0, 165, 255) : cv::Scalar(128, 128, 128), 1);
    }

    return result;
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

    cv::Mat frame, fore;
    std::string lastGesture = "NONE";
    int stableCount = 0;
    int frameCount = 0;
    float emaAngle = 0.0f;  // 策略3：一阶低通滤波平滑角度

    // 时域皮肤均值 — 吸收静态皮肤（人脸/背景），只保留移动的手
    cv::Mat avgSkin;  // CV_32F, 320x240, 指数移动平均

    float avgBrightness = 128.0f;

    while (g_running) {
        if (!cap.read(frame)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        cv::flip(frame, frame, 1);

        float rawAngle = 0.0f;
        std::string rawGesture = "NONE";

        cv::Scalar meanBGR = cv::mean(frame);
        avgBrightness = 0.299f * (float)meanBGR[2] + 0.587f * (float)meanBGR[1] + 0.114f * (float)meanBGR[0];

        // Downscale for processing speed (4x fewer pixels)
        cv::Mat small;
        cv::resize(frame, small, cv::Size(320, 240));
        detectSkin(small, fore);  // binary mask (0/255)

        // ── 时域运动分离：只保留移动的皮肤（手），排除静止皮肤（脸/背景）──
        // 原理：对皮肤掩码做指数移动平均，人脸和背景肤色始终在场 →
        // 被均值吸收后差值接近0；手部出现/移动 → 新皮肤像素 → 差值高
        cv::Mat foreF;
        fore.convertTo(foreF, CV_32F);  // 0.0 or 255.0

        if (avgSkin.empty()) {
            avgSkin = foreF.clone();  // 首帧直接初始化
        }
        // 移动皮肤 = 当前皮肤 - 时间平均皮肤（新出现的皮肤区域）
        cv::Mat movingSkin;
        cv::subtract(foreF, avgSkin, movingSkin);
        // 只保留显著差异（>80/255）→ 这才是手
        cv::threshold(movingSkin, fore, 80.0, 255.0, cv::THRESH_BINARY);
        fore.convertTo(fore, CV_8U);
        // 缓慢更新均值 (α=0.03)，静态皮肤约2秒内被完全吸收
        cv::accumulateWeighted(foreF, avgSkin, 0.03);
        // ────────────────────────────────────────────────────────────────

        if (cv::countNonZero(fore) > 400) {
            std::vector<std::vector<cv::Point>> contours;
            cv::findContours(fore, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

            // Fast pre-filter: pick best by area + position + shape
            struct Candidate { int idx; double area; double cy; };
            std::vector<Candidate> cands;
            for (int i = 0; i < (int)contours.size(); i++) {
                double a = cv::contourArea(contours[i]);
                if (a < 1000) continue;
                cv::Moments m = cv::moments(contours[i]);
                if (m.m00 <= 0) continue;
                cands.push_back({i, a, m.m01 / m.m00});
            }
            std::sort(cands.begin(), cands.end(), [](const Candidate& a, const Candidate& b) {
                double wa = a.area * (a.cy > 120 ? 2.0 : a.cy > 80 ? 1.0 : 0.2);
                double wb = b.area * (b.cy > 120 ? 2.0 : b.cy > 80 ? 1.0 : 0.2);
                return wa > wb;
            });
            if (cands.size() > 5) cands.resize(5);

            int best = -1;
            double bestScore = 0;
            for (const auto& c : cands) {
                std::vector<cv::Point> hPts;
                cv::convexHull(contours[c.idx], hPts, false, false);
                double hullArea = cv::contourArea(hPts);
                if (hullArea <= 0) continue;
                double solidity = c.area / hullArea;
                if (solidity < 0.35 || solidity > 0.92) continue;
                cv::Rect br = cv::boundingRect(contours[c.idx]);
                double ar = (double)br.width / (double)(std::max)(1, br.height);
                if (ar < 0.2 || ar > 4.0) continue;
                double posW = (c.cy > 120) ? 2.0 : (c.cy > 80 ? 1.0 : 0.2);
                double score = c.area * posW;
                if (score > bestScore) { bestScore = score; best = c.idx; }
            }

            if (best >= 0) {
                // Crop mask around best contour, re-extract with NONE for accurate landmark extraction
                cv::Rect roi = cv::boundingRect(contours[best]);
                roi.x = (std::max)(0, roi.x - 10);
                roi.y = (std::max)(0, roi.y - 10);
                roi.width = (std::min)(fore.cols - roi.x, roi.width + 20);
                roi.height = (std::min)(fore.rows - roi.y, roi.height + 20);
                cv::Mat roiMask = fore(roi);

                std::vector<std::vector<cv::Point>> detailContours;
                cv::findContours(roiMask, detailContours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
                if (detailContours.empty()) detailContours.push_back(contours[best]);

                int dBest = 0;
                double dMax = 0;
                for (int i = 0; i < (int)detailContours.size(); i++) {
                    double a = cv::contourArea(detailContours[i]);
                    if (a > dMax) { dMax = a; dBest = i; }
                }

                // Offset ROI contour back to full small-frame coords, then scale up
                const double sx = 2.0, sy = 2.0;
                std::vector<cv::Point> cntRaw = detailContours[dBest];
                for (auto& p : cntRaw) {
                    p.x = (int)((p.x + roi.x) * sx);
                    p.y = (int)((p.y + roi.y) * sy);
                }

                // Smooth NONE contour to prevent self-intersections
                // (self-intersecting contours crash convexityDefects with "indices are not monotonous")
                std::vector<cv::Point> cntFull;
                cv::approxPolyDP(cntRaw, cntFull, 1.5, true);

                // Raw angle from small-frame moments
                cv::Moments m = cv::moments(detailContours[dBest]);
                if (m.m00 > 0) {
                    float cx = (float)((m.m10 / m.m00 + roi.x) * sx);
                    float normX = (cx - 64.0f) / (576.0f - 64.0f);
                    normX = (std::max)(0.0f, (std::min)(1.0f, normX));
                    rawAngle = (normX - 0.5f) * 130.0f;
                }

                // 掌心圆分类（参照 vision/OpenCVHandGuesture）
                cv::Point palm;
                double radius = 0;
                rawGesture = classifyByPalmCircle(cntFull, palm, radius, lastGesture, &frame);

                // 策略3：角度 EMA 低通滤波（平滑系数 0.75）
                emaAngle = 0.75f * emaAngle + 0.25f * rawAngle;

                // Debug
                if (frameCount % 30 == 0) {
                    printf("[DEBUG] frame=%d gesture=%s rawAngle=%.1f emaAngle=%.1f radius=%.0f "
                           "brightness=%.0f pts=%d tips=%s\n",
                           frameCount, rawGesture.c_str(), rawAngle, emaAngle, radius,
                           avgBrightness, (int)cntFull.size(), rawGesture.c_str());
                }

                // Draw contour + hull
                std::vector<std::vector<cv::Point>> tc(1, cntFull);
                cv::drawContours(frame, tc, -1, cv::Scalar(0, 255, 0), 2);

                std::vector<int> hullIdx;
                cv::convexHull(cntFull, hullIdx, false, false);
                std::vector<std::vector<cv::Point>> hp(1);
                for (int h : hullIdx) hp[0].push_back(cntFull[h]);
                cv::drawContours(frame, hp, 0, cv::Scalar(0, 255, 255), 2);
            }
        } else {
            // No skin detected → drift EMA angle toward 0 (center)
            emaAngle *= 0.9f;
        }

        // ── 策略3：时域去抖 ──
        // 手势状态锁定：同一手势连续 3 帧才生效
        if (rawGesture == lastGesture && rawGesture != "NONE") {
            stableCount++;
        } else {
            stableCount = 0;
            lastGesture = rawGesture;
        }
        std::string lockedGesture = (stableCount >= 3) ? rawGesture : "NONE";

        // Angle indicator
        cv::line(frame, cv::Point(320, 0), cv::Point(320, 480), cv::Scalar(255, 0, 0), 1);
        cv::circle(frame, cv::Point(320 + (int)(emaAngle * 3), 240), 10,
                   lockedGesture == "FIST" ? cv::Scalar(0, 0, 255) :
                   lockedGesture == "OPEN_PALM" ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255), -1);

        // Status overlay
        cv::putText(frame, lockedGesture, cv::Point(10, 25),
                    cv::FONT_HERSHEY_COMPLEX_SMALL, 1.0,
                    lockedGesture == "FIST" ? cv::Scalar(0, 165, 255) :
                    lockedGesture == "OPEN_PALM" ? cv::Scalar(0, 255, 0) : cv::Scalar(128, 128, 128), 1);

        // Brightness indicator
        char buf[32];
        snprintf(buf, sizeof(buf), "B:%.0f", avgBrightness);
        cv::putText(frame, buf, cv::Point(560, 25),
                    cv::FONT_HERSHEY_COMPLEX_SMALL, 0.6, cv::Scalar(200, 200, 200), 1);

        frameCount++;

        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_angle = emaAngle;
            // 只有锁定状态的手势才发布
            if (lockedGesture != "NONE") g_gesture = lockedGesture;
            g_frame = frame.clone();
            cv::imencode(".jpg", g_frame, g_jpegBytes, {cv::IMWRITE_JPEG_QUALITY, 50});
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
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
#videoFeed{border:3px solid #ffd700;border-radius:12px;width:360px;height:270px;object-fit:cover}
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
<div><img id="videoFeed" src="/frame" alt="Camera"></div>
<div id="camMsg" style="color:#4caf50;font-size:13px;margin:4px 0">Camera OK</div>
<div class="info-panel">
<div class="info-item"><div class="info-label">Gesture</div><div class="info-value g-none" id="gv">NONE</div></div>
<div class="info-item"><div class="info-label">Hook Angle</div><div class="info-value" id="av">0.0</div></div>
<div class="info-item"><div class="info-label">Status</div><div class="info-value" id="sv"><span class="dot dot-off"></span>OFF</div></div>
</div>
<div class="angle-bar"><div class="angle-fill" id="ab"></div></div>
<div class="angle-labels"><span>-65</span><span>0</span><span>+65</span></div>
<script>
// Frame refresh — preload then swap (avoids request cancellation)
var feed=document.getElementById('videoFeed'),msg=document.getElementById('camMsg');
(function loop(){
var pre=new Image();
pre.onload=function(){
feed.src=pre.src;
msg.textContent='Camera OK';
msg.style.color='#4caf50';
};
pre.src='/frame?t='+Date.now();
setTimeout(loop,200);
})();

// Gesture polling
setInterval(function(){try{
var x=new XMLHttpRequest();
x.open('GET','/gesture',true);
x.onload=function(){try{
var d=JSON.parse(x.responseText);
document.getElementById('gv').textContent=d.gesture;
document.getElementById('gv').className='info-value g-'+(d.gesture==='OPEN_PALM'?'open':d.gesture==='FIST'?'fist':'none');
document.getElementById('av').textContent=d.angle.toFixed(1);
document.getElementById('sv').innerHTML='<span class="dot '+(d.connected?'dot-on':'dot-off')+'"></span>'+(d.connected?'ON':'OFF');
document.getElementById('ab').style.left=Math.min(100,Math.max(0,((d.angle+65)/130)*100))+'%';
}catch(e){}}
x.send()
}catch(e){}},80)
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
    std::string rawPath = request.substr(p1 + 1, p2 - p1 - 1);
    // Strip query string (e.g. "/frame?t=123" → "/frame")
    size_t qpos = rawPath.find('?');
    std::string path = (qpos != std::string::npos) ? rawPath.substr(0, qpos) : rawPath;

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
        std::vector<unsigned char> jpeg;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            jpeg = g_jpegBytes;
        }
        if (jpeg.empty()) {
            sendAll(client, "HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n");
        } else {
            std::ostringstream oss;
            oss << "HTTP/1.1 200 OK\r\n"
                << "Content-Type: image/jpeg\r\n"
                << "Access-Control-Allow-Origin: *\r\n"
                << "Cache-Control: no-cache\r\n"
                << "Connection: close\r\n"
                << "Content-Length: " << jpeg.size() << "\r\n\r\n";
            std::string hdr = oss.str();
            // Single send call: header + body in one shot, no extra copy
            hdr.append(reinterpret_cast<const char*>(jpeg.data()), jpeg.size());
            sendAll(client, hdr);
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
            std::vector<unsigned char> jpeg;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                jpeg = g_jpegBytes;
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
