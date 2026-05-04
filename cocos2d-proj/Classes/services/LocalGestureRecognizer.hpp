#pragma once
#include "IGestureRecognizer.hpp"
#include "GestureClient.hpp"
#include <thread>
#include <atomic>
#include <mutex>

/// 本地高频识别器（"小脑"）—— 封装 GestureServer.exe 的 HTTP 客户端
///
/// 架构：
///   - 启动 GestureServer.exe（独立进程，OpenCV 摄像头 + 手势检测）
///   - 通过 HTTP 轮询 /gesture 端点获取实时手势 + 角度
///   - 将 GestureData 转换为统一的 GestureResult 格式
///
/// 精度策略（在 GestureServer 已实现，此处为客户端封装）：
///   策略1 — 多模态关键点融合（凸包指尖提取 + 轮廓距离）
///   策略2 — 指尖距离算法（替代置信度阈值）
///   策略3 — 时域平滑滤波（EMA 角度 + 3帧状态锁定）
///   策略4 — 动态亮度自适应（HSV 阈值随亮度调整）

class LocalGestureRecognizer : public IGestureRecognizer {
public:
    static LocalGestureRecognizer* getInstance();

    bool start() override;
    void stop() override;
    void setCallback(ResultCallback cb) override { _callback = std::move(cb); }
    void pushFrame(const std::vector<uint8_t>& jpegData) override;  // no-op：GestureServer 自行取帧
    bool isRunning() const override { return _running; }
    const char* name() const override { return "local"; }

private:
    LocalGestureRecognizer() = default;
    ~LocalGestureRecognizer();

    void pollThread();

    std::thread _thread;
    std::atomic<bool> _running{false};
    ResultCallback _callback;
    std::string _lastGesture;
    int _stableCount = 0;
};
