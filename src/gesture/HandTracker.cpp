#include "HandTracker.h"
#include "network/AIVisionClient.h"
#include <QDebug>
#include <cmath>

#ifdef HAS_OPENCV
#include <opencv2/imgproc.hpp>
#endif

HandTracker::HandTracker(QObject *parent)
    : QObject(parent)
{
    m_aiClient = new AIVisionClient(this);

    // 将 processLoop 移到工作线程
    this->moveToThread(&m_workThread);
}

HandTracker::~HandTracker()
{
    stop();
}

// ==================== 线程控制 ====================

void HandTracker::start()
{
    QMutexLocker lock(&m_mutex);
    if (m_running) return;

    m_running = true;

#ifdef HAS_OPENCV
    m_workThread.start();
    QMetaObject::invokeMethod(this, "processLoop", Qt::QueuedConnection);
    emit trackingStarted();
#else
    qWarning() << "OpenCV 未安装，手势识别不可用。请使用键盘模式。";
    emit modeSwitchFailed("OpenCV not available");
#endif
}

void HandTracker::stop()
{
    {
        QMutexLocker lock(&m_mutex);
        if (!m_running) return;
        m_running = false;
    }

    m_workThread.quit();
    m_workThread.wait();
    emit trackingStopped();
}

bool HandTracker::isRunning() const
{
    QMutexLocker lock(&const_cast<HandTracker*>(this)->m_mutex);
    return m_running;
}

void HandTracker::setMode(Mode mode)
{
    QMutexLocker lock(&m_mutex);
    m_mode = mode;
}

// ==================== OpenCV 处理循环 ====================

#ifdef HAS_OPENCV

void HandTracker::processLoop()
{
    cv::VideoCapture cap(m_cameraIndex);
    if (!cap.isOpened()) {
        qWarning() << "无法打开摄像头 index:" << m_cameraIndex;
        emit modeSwitchFailed("Camera not available");
        m_running = false;
        return;
    }

    cap.set(cv::CAP_PROP_FRAME_WIDTH, m_frameWidth);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, m_frameHeight);

    cv::Mat frame;
    while (m_running) {
        cap >> frame;
        if (frame.empty()) continue;

        if (m_mode == LocalCV) {
            processFrame(frame);
        } else {
            // AI 模式：将帧发送给 AIVisionClient
            // m_aiClient->analyzeFrame(frame); // 由 AIVisionClient 处理
        }

        // 短暂休眠减少 CPU 占用
        QThread::msleep(16); // ~60fps
    }

    cap.release();
}

void HandTracker::processFrame(const cv::Mat &frame)
{
    // 转为 HSV 色彩空间
    cv::Mat hsv;
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);

    // 提取手部掩码
    cv::Mat mask = extractHandMask(hsv);

    // 形态学滤波：去除噪点
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);

    // 找手部边界框
    cv::Rect bbox = findHandBoundingBox(mask);
    if (bbox.area() < 500) {
        // 手部太小或未检测到
        return;
    }

    // 计算倾斜角度
    qreal tilt = computeTiltAngle(mask, bbox);

    // 低通滤波
    m_filteredAngle = kAlpha * tilt + (1.0 - kAlpha) * m_filteredAngle;
    emit handTilt(m_filteredAngle);

    // 检测手势（带连续帧确认防抖）
    bool openPalm = detectOpenPalm(mask, bbox);
    bool fist     = detectFist(mask, bbox);
    bool thumbsUp = detectThumbsUp(mask, bbox);

    if (openPalm) {
        m_openPalmCount++;
        if (m_openPalmCount >= kGestureThreshold && !m_wasOpenPalm) {
            m_wasOpenPalm = true;
            emit handOpen();
        }
    } else {
        m_openPalmCount = 0;
        m_wasOpenPalm = false;
    }

    if (fist) {
        m_fistCount++;
        if (m_fistCount >= kGestureThreshold && !m_wasFist) {
            m_wasFist = true;
            emit handFist();
        }
    } else {
        m_fistCount = 0;
        m_wasFist = false;
    }

    if (thumbsUp) {
        emit handGesture("thumbs_up");
    }
}

// ==================== 颜色阈值提取手部区域 ====================

cv::Mat HandTracker::extractHandMask(const cv::Mat &hsv)
{
    cv::Mat mask;
    // 根据 HSV 阈值提取手部区域
    cv::Scalar lower(m_hueLow, m_satLow, m_valLow);
    cv::Scalar upper(m_hueHigh, 255, 255);
    cv::inRange(hsv, lower, upper, mask);
    return mask;
}

cv::Rect HandTracker::findHandBoundingBox(const cv::Mat &mask)
{
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask.clone(), contours, cv::RETR_EXTERNAL,
                     cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) return cv::Rect();

    // 找最大轮廓
    double maxArea = 0;
    int maxIdx = -1;
    for (size_t i = 0; i < contours.size(); ++i) {
        double area = cv::contourArea(contours[i]);
        if (area > maxArea) {
            maxArea = area;
            maxIdx = static_cast<int>(i);
        }
    }

    if (maxIdx < 0) return cv::Rect();
    return cv::boundingRect(contours[maxIdx]);
}

// ==================== 手势分析 ====================

qreal HandTracker::computeTiltAngle(const cv::Mat &mask, const cv::Rect &bbox)
{
    // 在边界框区域计算手部的主轴方向（使用 PCA 或力矩）
    cv::Mat roi = mask(bbox);

    // 使用图像矩计算方向
    cv::Moments m = cv::moments(roi, true);
    if (m.mu20 + m.mu02 < 1e-6) return 0.0;

    // 计算主轴角度（通过中心矩）
    double theta = 0.5 * std::atan2(2.0 * m.mu11, m.mu20 - m.mu02);
    qreal angleDeg = static_cast<qreal>(theta * 180.0 / CV_PI);

    // 映射到钩子摆动范围 [-65, 65]
    angleDeg = qBound(-65.0, angleDeg, 65.0);
    return angleDeg;
}

bool HandTracker::detectOpenPalm(const cv::Mat &mask, const cv::Rect &bbox)
{
    // 检测手掌张开：轮廓面积较大且凸包缺陷多
    std::vector<std::vector<cv::Point>> contours;
    cv::Mat roi = mask(bbox);
    cv::findContours(roi.clone(), contours, cv::RETR_EXTERNAL,
                     cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) return false;

    double area = cv::contourArea(contours[0]);
    cv::Rect brect = cv::boundingRect(contours[0]);
    double rectArea = brect.area();
    if (rectArea < 1.0) return false;

    // 轮廓面积占边界框的比例：手掌张开时比例较低（手指间隙）
    double fillRatio = area / rectArea;

    // 凸包缺陷检测
    std::vector<int> hull;
    cv::convexHull(contours[0], hull);
    std::vector<cv::Vec4i> defects;
    if (hull.size() > 3) {
        cv::convexityDefects(contours[0], hull, defects);
    }

    // 手掌张开：填充率低 + 有多个凸包缺陷
    return (fillRatio < 0.7 && defects.size() >= 2);
}

bool HandTracker::detectFist(const cv::Mat &mask, const cv::Rect &bbox)
{
    // 检测握拳：轮廓面积较小，填充率高，接近圆形
    std::vector<std::vector<cv::Point>> contours;
    cv::Mat roi = mask(bbox);
    cv::findContours(roi.clone(), contours, cv::RETR_EXTERNAL,
                     cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) return false;

    double area = cv::contourArea(contours[0]);
    cv::Rect brect = cv::boundingRect(contours[0]);
    double rectArea = brect.area();
    if (rectArea < 1.0) return false;

    double fillRatio = area / rectArea;

    // 计算圆度
    double perimeter = cv::arcLength(contours[0], true);
    if (perimeter < 1.0) return false;
    double circularity = 4.0 * CV_PI * area / (perimeter * perimeter);

    // 拳头：填充率高 + 接近圆形 + 尺寸相对小
    return (fillRatio > 0.75 && circularity > 0.7);
}

bool HandTracker::detectThumbsUp(const cv::Mat &mask, const cv::Rect &bbox)
{
    // 检测点赞手势：高宽比大于 1.5（竖直方向的竖起拇指）
    double aspectRatio = static_cast<double>(bbox.height) / bbox.width;

    // 点赞手势特征：竖直细长的轮廓
    std::vector<std::vector<cv::Point>> contours;
    cv::Mat roi = mask(bbox);
    cv::findContours(roi.clone(), contours, cv::RETR_EXTERNAL,
                     cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) return false;

    // 寻找凸包缺陷（拇指与拳头之间的凹陷）
    std::vector<int> hull;
    cv::convexHull(contours[0], hull);
    std::vector<cv::Vec4i> defects;
    if (hull.size() > 3) {
        cv::convexityDefects(contours[0], hull, defects);
    }

    return (aspectRatio > 1.3 && defects.size() >= 1);
}

#endif // HAS_OPENCV
