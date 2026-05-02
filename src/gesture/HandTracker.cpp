#include "HandTracker.h"
#include "network/AIVisionClient.h"
#include <QDebug>
#include <cmath>

#ifdef HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>
#endif

HandTracker::HandTracker(QObject *parent)
    : QObject(parent)
{
    m_aiClient = new AIVisionClient(this);
    moveToThread(&m_workThread);
}

HandTracker::~HandTracker()
{
    stop();
}

void HandTracker::start()
{
    QMutexLocker lock(&m_mutex);
    if (m_running) return;

#ifdef HAS_OPENCV
    m_running = true;
    m_workThread.start();
    QMetaObject::invokeMethod(this, "processLoop", Qt::QueuedConnection);
    emit trackingStarted();
#else
    qWarning() << "OpenCV 未安装，手势识别不可用";
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
    QMutexLocker lock(&m_mutex);
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
        {
            QMutexLocker lock(&m_mutex);
            if (!m_running) break;
        }

        cap >> frame;
        if (frame.empty()) continue;

        if (m_mode == LocalCV) {
            processFrame(frame);
        } else {
            // AI 模式：编码帧为 Base64 并发送到视觉 API
            std::vector<uchar> buf;
            cv::imencode(".jpg", frame, buf);
            QByteArray base64 = QByteArray::fromRawData(
                reinterpret_cast<const char*>(buf.data()),
                static_cast<int>(buf.size())).toBase64();
            m_aiClient->analyzeFrame(base64);
        }

        QThread::msleep(16);
    }

    cap.release();
}

void HandTracker::processFrame(const cv::Mat &frame)
{
    cv::Mat hsv;
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);

    cv::Mat mask = extractHandMask(hsv);

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);

    cv::Rect bbox = findHandBoundingBox(mask);
    if (bbox.area() < 500) return;

    // 倾斜角度
    qreal tilt = computeTiltAngle(mask, bbox);
    m_filteredAngle = 0.25 * tilt + 0.75 * m_filteredAngle;
    emit handTilt(m_filteredAngle);

    // 手势检测（带连续帧确认防抖）
    bool openPalm  = detectOpenPalm(mask, bbox);
    bool fist      = detectFist(mask, bbox);
    bool thumbsUp  = detectThumbsUp(mask, bbox);

    // 手掌张开
    if (openPalm) {
        if (++m_openPalmCount >= kGestureThreshold && !m_wasOpenPalm) {
            m_wasOpenPalm = true;
            emit handOpen();
        }
    } else {
        m_openPalmCount = 0;
        m_wasOpenPalm = false;
    }

    // 握拳
    if (fist) {
        if (++m_fistCount >= kGestureThreshold && !m_wasFist) {
            m_wasFist = true;
            emit handFist();
        }
    } else {
        m_fistCount = 0;
        m_wasFist = false;
    }

    // 点赞（带防抖）
    if (thumbsUp) {
        if (++m_thumbsUpCount >= kGestureThreshold && !m_wasThumbsUp) {
            m_wasThumbsUp = true;
            emit handGesture("thumbs_up");
        }
    } else {
        m_thumbsUpCount = 0;
        m_wasThumbsUp = false;
    }
}

// ==================== 颜色阈值 ====================

cv::Mat HandTracker::extractHandMask(const cv::Mat &hsv)
{
    cv::Mat mask;
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

    double maxArea = 0;
    int maxIdx = -1;
    for (size_t i = 0; i < contours.size(); ++i) {
        double area = cv::contourArea(contours[i]);
        if (area > maxArea) { maxArea = area; maxIdx = static_cast<int>(i); }
    }

    if (maxIdx < 0) return cv::Rect();
    return cv::boundingRect(contours[maxIdx]);
}

// ==================== 手势分析 ====================

qreal HandTracker::computeTiltAngle(const cv::Mat &mask, const cv::Rect &bbox)
{
    cv::Mat roi = mask(bbox);
    cv::Moments m = cv::moments(roi, true);
    if (m.mu20 + m.mu02 < 1e-6) return 0.0;

    double theta = 0.5 * std::atan2(2.0 * m.mu11, m.mu20 - m.mu02);
    qreal deg = static_cast<qreal>(theta * 180.0 / CV_PI);
    return qBound(-65.0, deg, 65.0);
}

bool HandTracker::detectOpenPalm(const cv::Mat &mask, const cv::Rect &bbox)
{
    std::vector<std::vector<cv::Point>> contours;
    cv::Mat roi = mask(bbox);
    cv::findContours(roi.clone(), contours, cv::RETR_EXTERNAL,
                     cv::CHAIN_APPROX_SIMPLE);
    if (contours.empty()) return false;

    double area = cv::contourArea(contours[0]);
    double rectArea = bbox.area();
    if (rectArea < 1.0) return false;

    double fillRatio = area / rectArea;

    std::vector<int> hull;
    cv::convexHull(contours[0], hull);
    std::vector<cv::Vec4i> defects;
    if (hull.size() > 3)
        cv::convexityDefects(contours[0], hull, defects);

    return (fillRatio < 0.7 && defects.size() >= 2);
}

bool HandTracker::detectFist(const cv::Mat &mask, const cv::Rect &bbox)
{
    std::vector<std::vector<cv::Point>> contours;
    cv::Mat roi = mask(bbox);
    cv::findContours(roi.clone(), contours, cv::RETR_EXTERNAL,
                     cv::CHAIN_APPROX_SIMPLE);
    if (contours.empty()) return false;

    double area = cv::contourArea(contours[0]);
    double rectArea = bbox.area();
    if (rectArea < 1.0) return false;

    double fillRatio = area / rectArea;
    double perimeter = cv::arcLength(contours[0], true);
    if (perimeter < 1.0) return false;
    double circularity = 4.0 * CV_PI * area / (perimeter * perimeter);

    return (fillRatio > 0.75 && circularity > 0.7);
}

bool HandTracker::detectThumbsUp(const cv::Mat &mask, const cv::Rect &bbox)
{
    double aspectRatio = static_cast<double>(bbox.height) / bbox.width;

    std::vector<std::vector<cv::Point>> contours;
    cv::Mat roi = mask(bbox);
    cv::findContours(roi.clone(), contours, cv::RETR_EXTERNAL,
                     cv::CHAIN_APPROX_SIMPLE);
    if (contours.empty()) return false;

    std::vector<int> hull;
    cv::convexHull(contours[0], hull);
    std::vector<cv::Vec4i> defects;
    if (hull.size() > 3)
        cv::convexityDefects(contours[0], hull, defects);

    return (aspectRatio > 1.3 && defects.size() >= 1);
}

#endif // HAS_OPENCV
