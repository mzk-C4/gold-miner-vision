#pragma once
#include "IGestureRecognizer.hpp"
#include "GestureData.hpp"
#include <memory>
#include <atomic>
#include <mutex>
#include <functional>
#include <chrono>

class LocalGestureRecognizer;
class CloudGestureRecognizer;
class AIGestureRecognizer;

/// 手势融合器 —— 双通道混合识别架构的核心
///
/// ┌─────────────────────────────────────────────────────┐
/// │                  GestureFusion                       │
/// │  ┌───────────────┐    ┌──────────────────┐          │
/// │  │ Local (高频)   │    │ Cloud (低频)      │          │
/// │  │ 每 ~30ms 产出  │    │ 仅在 FIST 锁定时   │          │
/// │  │ 角度+手势      │    │ 异步发送做双保险   │          │
/// │  └───────┬───────┘    └────────┬─────────┘          │
/// │          │                     │                     │
/// │          └──────┬──────────────┘                     │
/// │                 ▼                                     │
/// │         GestureCommand                                │
/// │   (shouldReleaseHook, targetAngle)                    │
/// └─────────────────────────────────────────────────────┘
///
/// 融合逻辑：
///   - OPEN_PALM：本地角度直接输出（连续瞄准，无需云端校验）
///   - FIST 锁定：本地 + 云端双重确认后才释放钩子
///   - 云端超时/失败 → 降级：仅信任本地结果
///   - 云端与本地不一致 → 放弃本次触发（安全优先）

class GestureFusion {
public:
    using CommandCallback = std::function<void(const GestureCommand&)>;

    static GestureFusion* getInstance();

    /// 初始化并启动双通道（本地 OpenCV + 可选云端校验）
    bool initialize(const std::string& cloudEndpointId, const std::string& cloudApiKey);

    /// 初始化 AI 独占模式（摄像头直连大模型，跳过 OpenCV/MediaPipe）
    bool initializeAI(const std::string& endpointId, const std::string& apiKey);

    void shutdown();

    /// 注册指令回调（发往游戏逻辑）
    void setCommandCallback(CommandCallback cb) { _cmdCallback = std::move(cb); }

    /// 每帧由 Game::update() 调用，产出 GestureCommand
    GestureCommand tick(float dt);

    /// 推送相机预览帧（由 Game::updateCameraPreview 调用）
    void pushPreviewFrame(const std::vector<uint8_t>& jpegData);

    /// 状态查询（调试/UI）
    bool isLocalRunning() const;
    bool isCloudEnabled() const;
    std::string statusString() const;

private:
    GestureFusion() = default;
    ~GestureFusion();

    void onLocalResult(const GestureResult& result);
    void onCloudResult(const GestureResult& result);

    CommandCallback _cmdCallback;

    // 最新本地结果（高频更新）
    GestureResult _latestLocal;
    mutable std::mutex _localMutex;

    // 最新云端结果（低频更新）
    GestureResult _latestCloud;
    mutable std::mutex _cloudMutex;

    // 待确认的 FIST 触发
    std::atomic<bool> _fistPendingCloud{false};
    std::chrono::steady_clock::time_point _fistPendingTime;
    std::mutex _pendingMutex;

    // 角度 EMA（本地已做一次，这里再做一层游戏级平滑）
    float _gameAngle = 0.0f;

    // 冷却：两次释放钩子之间最小间隔
    float _releaseCooldown = 0.0f;
    static constexpr float COOLDOWN_DURATION = 0.8f;

    // 长时间握拳计时器：超过阈值引爆而非释放钩子
    float _fistHoldTime = 0.0f;
    bool  _fistHeldLast = false;  // 上一帧是否 FIST（检测上升沿/下降沿）
    static constexpr float BOMB_HOLD_DURATION = 1.5f;

    bool _initialized = false;
    bool _aiMode = false;  // true: AI 独占模式，false: 本地+云端双通道
};
