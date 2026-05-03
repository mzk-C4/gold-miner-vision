#include "GestureRecognizer.hpp"
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

USING_NS_CC;

static GestureRecognizer* s_instance = nullptr;

GestureRecognizer* GestureRecognizer::getInstance() {
    if (!s_instance) {
        s_instance = new GestureRecognizer();
    }
    return s_instance;
}

GestureRecognizer::~GestureRecognizer() {
    stop();
}

void GestureRecognizer::start(int cameraId) {
    if (_running) return;
    _running = true;
    _thread = std::thread(&GestureRecognizer::processLoop, this, cameraId);
}

void GestureRecognizer::stop() {
    _running = false;
    if (_thread.joinable()) {
        _thread.join();
    }
}

bool GestureRecognizer::detectSkin(const cv::Mat& frame, cv::Mat& mask) {
    cv::Mat hsv, ycrcb;
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);
    cv::cvtColor(frame, ycrcb, cv::COLOR_BGR2YCrCb);

    // HSV skin range
    cv::Mat hsvMask;
    cv::inRange(hsv, cv::Scalar(0, 20, 70), cv::Scalar(25, 170, 255), hsvMask);

    // YCrCb skin range
    cv::Mat ycrcbMask;
    cv::inRange(ycrcb, cv::Scalar(0, 135, 85), cv::Scalar(255, 180, 135), ycrcbMask);

    // Combine masks
    cv::bitwise_and(hsvMask, ycrcbMask, mask);

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::erode(mask, mask, kernel);
    cv::dilate(mask, mask, kernel);
    cv::dilate(mask, mask, kernel);
    cv::erode(mask, mask, kernel);

    return cv::countNonZero(mask) > 1000;
}

Gesture GestureRecognizer::classifyHand(const std::vector<cv::Point>& contour) {
    double area = cv::contourArea(contour);
    if (area < 6000) return Gesture::NONE;

    std::vector<cv::Point> hull;
    cv::convexHull(contour, hull);

    std::vector<int> hullIndices;
    cv::convexHull(contour, hullIndices, false, false);

    std::vector<cv::Vec4i> defects;
    if (hullIndices.size() > 3) {
        cv::convexityDefects(contour, hullIndices, defects);
    }

    // Count significant convexity defects (finger gaps)
    int fingerDefects = 0;
    for (const auto& d : defects) {
        float depth = d[3] / 256.0f;
        if (depth > 20.0f && depth < 200.0f) {
            fingerDefects++;
        }
    }

    if (fingerDefects >= 2) return Gesture::OPEN_PALM;
    return Gesture::FIST;
}

void GestureRecognizer::processLoop(int cameraId) {
    cv::VideoCapture cap(cameraId);
    if (!cap.isOpened()) {
        CCLOG("GestureRecognizer: Cannot open camera %d", cameraId);
        _running = false;
        return;
    }

    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    Gesture lastGesture = Gesture::NONE;
    int gestureStableFrames = 0;
    const int STABLE_THRESHOLD = 8;

    cv::Mat frame;
    while (_running) {
        if (!cap.read(frame)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // Mirror for natural interaction
        cv::flip(frame, frame, 1);

        cv::Mat skinMask;
        Gesture detected = Gesture::NONE;

        if (detectSkin(frame, skinMask)) {
            std::vector<std::vector<cv::Point>> contours;
            cv::findContours(skinMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

            for (const auto& cnt : contours) {
                Gesture g = classifyHand(cnt);
                if (g != Gesture::NONE) {
                    detected = g;
                    break;
                }
            }
        }

        // Stability filter
        if (detected == lastGesture) {
            gestureStableFrames++;
        } else {
            gestureStableFrames = 0;
            lastGesture = detected;
        }

        // Fire callback on stable gesture
        if (gestureStableFrames == STABLE_THRESHOLD && _callback) {
            Director::getInstance()->getScheduler()->performFunctionInCocosThread(
                [gesture = detected, this]() {
                    if (_callback) _callback(gesture);
                }
            );
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    cap.release();
}
