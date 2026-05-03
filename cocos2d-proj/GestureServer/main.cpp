/// GestureServer — standalone x64 process using OpenCV for camera + hand tracking.
/// Communicates with the main Win32 game via named pipe: \\.\pipe\GoldMinerGesture
///
/// Protocol (text lines):
///   ANGLE <float>       — hook angle in degrees (-65..65), from hand X position
///   GESTURE <type>      — OPEN_PALM | FIST | NONE
///   FRAME <size>        — followed by <size> bytes of JPEG for camera preview
///   HEARTBEAT           — periodic keepalive

#include <opencv2/opencv.hpp>
#define NOMINMAX
#include <windows.h>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <algorithm>

static const char* PIPE_NAME = "\\\\.\\pipe\\GoldMinerGesture";
static std::atomic<bool> g_running{true};

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

static std::string classifyGesture(const std::vector<cv::Point>& contour) {
    double area = cv::contourArea(contour);
    if (area < 6000) return "NONE";
    std::vector<int> hullIndices;
    cv::convexHull(contour, hullIndices, false, false);
    std::vector<cv::Vec4i> defects;
    if (hullIndices.size() > 3) cv::convexityDefects(contour, hullIndices, defects);
    int fingerDefects = 0;
    for (const auto& d : defects) {
        float depth = d[3] / 256.0f;
        if (depth > 20.0f && depth < 200.0f) fingerDefects++;
    }
    return (fingerDefects >= 2) ? "OPEN_PALM" : "FIST";
}

static bool writePipe(HANDLE pipe, const std::string& msg) {
    DWORD written;
    return WriteFile(pipe, msg.c_str(), (DWORD)msg.size(), &written, nullptr) != 0;
}

int main() {
    cv::VideoCapture cap(0, cv::CAP_DSHOW);
    if (!cap.isOpened()) {
        fprintf(stderr, "GestureServer: cannot open camera\n");
        return 1;
    }
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    HANDLE pipe = CreateNamedPipeA(PIPE_NAME, PIPE_ACCESS_OUTBOUND,
        PIPE_TYPE_BYTE | PIPE_WAIT, 1, 65536, 1024, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "GestureServer: cannot create pipe\n");
        return 1;
    }

    printf("GestureServer: waiting for client...\n");
    if (!ConnectNamedPipe(pipe, nullptr)) {
        fprintf(stderr, "GestureServer: client connect failed\n");
        CloseHandle(pipe);
        return 1;
    }
    printf("GestureServer: client connected\n");

    DWORD pipeMode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
    SetNamedPipeHandleState(pipe, &pipeMode, nullptr, nullptr);

    cv::Mat frame, skinMask;
    std::string lastGesture = "NONE";
    int stableCount = 0;
    auto lastHeartbeat = std::chrono::steady_clock::now();
    auto lastFrame = std::chrono::steady_clock::now();

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

            if (bestIdx >= 0) {
                cv::Moments m = cv::moments(contours[bestIdx]);
                if (m.m00 > 0) {
                    float cx = (float)(m.m10 / m.m00);
                    float normX = (cx - 64.0f) / (576.0f - 64.0f);
                    normX = std::max(0.0f, std::min(1.0f, normX));
                    angle = (normX - 0.5f) * 130.0f;
                }
                gesture = classifyGesture(contours[bestIdx]);
            }
        }

        if (gesture == lastGesture) stableCount++;
        else { stableCount = 0; lastGesture = gesture; }

        char buf[64];
        snprintf(buf, sizeof(buf), "ANGLE %.1f\n", angle);
        writePipe(pipe, buf);

        if (stableCount == 6) {
            snprintf(buf, sizeof(buf), "GESTURE %s\n", gesture.c_str());
            writePipe(pipe, buf);
        }

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFrame).count() >= 100) {
            lastFrame = now;

            // Draw hand contours + angle indicator on preview frame
            if (!skinMask.empty()) {
                std::vector<std::vector<cv::Point>> contours;
                cv::findContours(skinMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
                for (size_t i = 0; i < contours.size(); i++)
                    cv::drawContours(frame, contours, (int)i, cv::Scalar(0, 255, 0), 2);
            }
            cv::line(frame, cv::Point(320, 0), cv::Point(320, 480), cv::Scalar(255, 0, 0), 1);
            int angleX = 320 + (int)(angle * 3);
            cv::circle(frame, cv::Point(angleX, 240), 10, cv::Scalar(0, 0, 255), -1);

            std::vector<unsigned char> jpegBuf;
            cv::imencode(".jpg", frame, jpegBuf, {cv::IMWRITE_JPEG_QUALITY, 40});

            snprintf(buf, sizeof(buf), "FRAME %d\n", (int)jpegBuf.size());
            DWORD written;
            WriteFile(pipe, buf, (DWORD)strlen(buf), &written, nullptr);
            WriteFile(pipe, jpegBuf.data(), (DWORD)jpegBuf.size(), &written, nullptr);
        }

        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastHeartbeat).count() >= 2000) {
            lastHeartbeat = now;
            writePipe(pipe, "HEARTBEAT\n");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    cap.release();
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
    return 0;
}
