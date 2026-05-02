/**
 * HandTracker — 手势识别追踪器（工作在独立 QThread）
 *
 * 模式A（本地CV）：OpenCV 颜色手套法，HSV 阈值提取手部区域
 * 模式B（AI视觉）：将帧编码为 Base64 调用豆包多模态 API
 * 所有手势不依赖手在画面中的绝对位置
 * 低通滤波消除抖动，连续帧确认防误触
 */
#ifndef HANDTRACKER_H
#define HANDTRACKER_H

#include <QObject>
#include <QThread>
#include <QMutex>

#ifdef HAS_OPENCV
#include <opencv2/core.hpp>
#endif

class AIVisionClient;

class HandTracker : public QObject
{
    Q_OBJECT

public:
    enum Mode { LocalCV, AIVision };

    explicit HandTracker(QObject *parent = nullptr);
    ~HandTracker() override;

    void start();
    void stop();

    void setMode(Mode mode);
    Mode mode() const { return m_mode; }
    bool isRunning() const;

signals:
    void handTilt(qreal angle);
    void handOpen();
    void handFist();
    void handGesture(const QString &name);

    void trackingStarted();
    void trackingStopped();
    void modeSwitchFailed(const QString &reason);

private:
#ifdef HAS_OPENCV
    Q_INVOKABLE void processLoop();
    void processFrame(const cv::Mat &frame);

    cv::Mat extractHandMask(const cv::Mat &hsv);
    cv::Rect findHandBoundingBox(const cv::Mat &mask);
    qreal computeTiltAngle(const cv::Mat &mask, const cv::Rect &bbox);
    bool  detectOpenPalm(const cv::Mat &mask, const cv::Rect &bbox);
    bool  detectFist(const cv::Mat &mask, const cv::Rect &bbox);
    bool  detectThumbsUp(const cv::Mat &mask, const cv::Rect &bbox);
#endif

    QThread m_workThread;
    mutable QMutex m_mutex;
    bool    m_running = false;
    Mode    m_mode    = LocalCV;

    AIVisionClient *m_aiClient = nullptr;

    int m_cameraIndex  = 0;
    int m_frameWidth   = 640;
    int m_frameHeight  = 480;

    // 低通滤波
    qreal m_filteredAngle = 0.0;

    // 手势防抖（连续 N 帧确认）
    bool m_wasOpenPalm    = false;
    bool m_wasFist        = false;
    bool m_wasThumbsUp    = false;
    int  m_openPalmCount  = 0;
    int  m_fistCount      = 0;
    int  m_thumbsUpCount  = 0;
    static constexpr int kGestureThreshold = 5;

    // 颜色阈值（蓝色手套默认）
    int m_hueLow  = 100;
    int m_hueHigh = 130;
    int m_satLow  = 80;
    int m_valLow  = 80;
};

#endif // HANDTRACKER_H
