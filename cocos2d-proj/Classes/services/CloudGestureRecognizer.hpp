#pragma once
#include "IGestureRecognizer.hpp"
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

/// 云端低频识别器（"大脑"）—— 封装豆包大模型异步请求
///
/// 职责：
///   - 接收来自融合器的帧（仅在本地锁定手势后触发）
///   - 异步发送给豆包视觉模型做二次校验
///   - 返回高置信度结果（confidence > 0.85 才采纳）
///
/// 策略5：增强提示词 —— 在 buildRequestBody 中实现

class CloudGestureRecognizer : public IGestureRecognizer {
public:
    static CloudGestureRecognizer* getInstance();

    bool start() override;
    void stop() override;
    void setCallback(ResultCallback cb) override { _callback = std::move(cb); }
    void pushFrame(const std::vector<uint8_t>& jpegData) override;
    bool isRunning() const override { return _running; }
    const char* name() const override { return "cloud"; }

    /// 配置 API 凭据
    void setEndpointId(const std::string& id) { _endpointId = id; }
    void setApiKey(const std::string& key) { _apiKey = key; }
    bool isEnabled() const { return !_endpointId.empty() && !_apiKey.empty(); }

private:
    CloudGestureRecognizer() = default;
    ~CloudGestureRecognizer();

    // 内部实现委托给 AIGestureService 的核心逻辑
    // 复用其 base64 编码 + HTTP POST + JSON 解析能力

    std::string _endpointId;
    std::string _apiKey;
    std::string _apiUrl = "https://ark.cn-beijing.volces.com/api/v3/chat/completions";
    ResultCallback _callback;
    std::atomic<bool> _running{false};
    std::atomic<bool> _requestInFlight{false};
};
