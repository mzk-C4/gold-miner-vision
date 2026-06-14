#pragma once

#include "cocos2d.h"
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <thread>
#include <atomic>
#include <functional>

enum class Gesture {
    NONE,
    OPEN_PALM,   // 张开手掌 — 放钩子
    FIST         // 握拳 — 准备
};

class GestureRecognizer {
public:
    using GestureCallback = std::function<void(Gesture)>;

    static GestureRecognizer* getInstance();

    void start(int cameraId = 0);
    void stop();
    bool isRunning() const { return _running; }

    void setCallback(GestureCallback cb) { _callback = std::move(cb); }

private:
    GestureRecognizer() = default;
    ~GestureRecognizer();

    void processLoop(int cameraId);

    Gesture classifyHand(const std::vector<cv::Point>& contour);
    bool detectSkin(const cv::Mat& frame, cv::Mat& mask);

    std::thread _thread;
    std::atomic<bool> _running{false};
    GestureCallback _callback;

    cv::Mat _latestFrame;
    std::mutex _frameMutex;
};
