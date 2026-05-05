#pragma once
#include "IGestureRecognizer.hpp"
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>

/// AI 视觉手势识别器 —— 大模型直连摄像头，取代 OpenCV/MediaPipe 检测管线
///
/// 架构：
///   - 摄像头采集：GestureServer.exe（仅 JPEG 画面输出，不做手势检测）
///   - 手势识别：Doubao Vision API（大模型直接分类 + 定位）
///   - 角度映射：归一化手部 X 位置 [-1,+1] → 钩子角度 [-65°,+65°]
///   - 结果通过 IGestureRecognizer 回调推入 GestureFusion 融合器
///
/// 识别逻辑与 OpenCV 路径一致：
///   - OPEN_PALM → 连续瞄准（角度由手部 X 位置决定）
///   - FIST → 握拳锁定，触发释放 / 引爆
///   - UNKNOWN → 维持当前状态

class AIGestureRecognizer : public IGestureRecognizer {
public:
    static AIGestureRecognizer* getInstance();

    bool start() override;
    void stop() override;
    void setCallback(ResultCallback cb) override { _callback = std::move(cb); }
    void pushFrame(const std::vector<uint8_t>& jpegData) override;
    bool isRunning() const override { return _running; }
    const char* name() const override { return "ai"; }

    /// 配置 API 凭据
    void setEndpointId(const std::string& id) { _endpointId = id; }
    void setApiKey(const std::string& key) { _apiKey = key; }
    bool isEnabled() const { return !_endpointId.empty() && !_apiKey.empty(); }

    /// 获取最新 JPEG 帧（供游戏内相机预览窗口使用）
    std::vector<uint8_t> getLatestFrame();

private:
    AIGestureRecognizer() = default;
    ~AIGestureRecognizer();

    void inferenceLoop();
    static std::string encodeB64(const std::vector<uint8_t>& data);

    std::string _endpointId;
    std::string _apiKey;
    std::string _apiUrl = "https://ark.cn-beijing.volces.com/api/v3/chat/completions";
    ResultCallback _callback;
    std::atomic<bool> _running{false};
    std::thread _thread;

    std::vector<uint8_t> _latestFrame;
    bool _hasFrame = false;
    std::mutex _frameMutex;
};
