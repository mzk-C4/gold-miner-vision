/**
 * HandTracker — 手势识别追踪器（工作在独立 QThread 中）
 *
 * 模式A（本地CV）：基于 OpenCV 颜色手套法（蓝/红色阈值提取手部区域）
 * 模式B（AI视觉）：调用 AIVisionClient 进行远程识别
 * 手势信号：手部倾斜角度 / 手掌张开 / 握拳 / 点赞
 * 所有手势不依赖手在画面中的绝对位置，只依赖姿态
 *
 * 低通滤波消除手部抖动
 * 通过 Qt 信号槽与 GameScene 通信（线程安全）
 */
#ifndef HANDTRACKER_H
#define HANDTRACKER_H

#include <QObject>
#include <QThread>
#include <QMutex>

#ifdef HAS_OPENCV
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#endif

class AIVisionClient;

class HandTracker : public QObject
{
    Q_OBJECT

public:
    explicit HandTracker(QObject *parent = nullptr);
    ~HandTracker() override;

    // ========== 线程控制 ==========
    void start();    // 启动摄像头线程
    void stop();     // 停止摄像头线程

    // ========== 模式控制 ==========
    enum Mode { LocalCV, AIVision };
    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    bool isRunning() const;

signals:
    // ========== 手势信号（连接到 GameScene） ==========
    void handTilt(qreal angle);            // 手部倾斜角度（-65 ~ +65 度）
    void handOpen();                       // 手掌张开 → 放钩
    void handFist();                       // 握拳 → 加速回收
    void handGesture(const QString &name); // 特定手势："thumbs_up", "ok"

    // ========== 状态信号 ==========
    void trackingStarted();
    void trackingStopped();
    void modeSwitchFailed(const QString &reason); // AI 模式降级通知

private:
#ifdef HAS_OPENCV
    Q_INVOKABLE void processLoop();        // 主循环（运行在工作线程）
    void processFrame(const cv::Mat &frame);

    // 颜色阈值提取
    cv::Mat extractHandMask(const cv::Mat &hsv);
    cv::Rect findHandBoundingBox(const cv::Mat &mask);

    // 手势分析
    qreal computeTiltAngle(const cv::Mat &mask, const cv::Rect &bbox);
    bool  detectOpenPalm(const cv::Mat &mask, const cv::Rect &bbox);
    bool  detectFist(const cv::Mat &mask, const cv::Rect &bbox);
    bool  detectThumbsUp(const cv::Mat &mask, const cv::Rect &bbox);
#endif

    // ========== 线程管理 ==========
    QThread  m_workThread;
    QMutex   m_mutex;
    bool     m_running = false;
    Mode     m_mode    = LocalCV;

    // ========== AI 客户端 ==========
    AIVisionClient *m_aiClient = nullptr;

    // ========== 摄像头参数 ==========
    int m_cameraIndex  = 0;
    int m_frameWidth   = 640;
    int m_frameHeight  = 480;

    // ========== 低通滤波 ==========
    qreal m_filteredAngle = 0.0;
    static constexpr qreal kAlpha = 0.25; // 滤波系数

    // ========== 手势防抖（避免重复触发） ==========
    bool m_wasOpenPalm   = false;
    bool m_wasFist       = false;
    int  m_openPalmCount = 0;     // 连续帧计数
    int  m_fistCount     = 0;
    static constexpr int kGestureThreshold = 5; // 连续 N 帧才触发

    // ========== 颜色阈值参数（蓝色手套默认） ==========
    // HSV 范围可调，用于适配不同颜色的手套
    int m_hueLow  = 100;  // 蓝色 H 分量下限
    int m_hueHigh = 130;  // 蓝色 H 分量上限
    int m_satLow  = 80;   // 饱和度下限
    int m_valLow  = 80;   // 明度下限
};

#endif // HANDTRACKER_H
