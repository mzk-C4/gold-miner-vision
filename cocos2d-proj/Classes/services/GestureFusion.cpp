#include "GestureFusion.hpp"
#include "LocalGestureRecognizer.hpp"
#include "CloudGestureRecognizer.hpp"
#include "GestureClient.hpp"
#include <mutex>

USING_NS_CC;

static GestureFusion* s_fusionInstance = nullptr;

GestureFusion* GestureFusion::getInstance() {
    if (!s_fusionInstance) s_fusionInstance = new GestureFusion();
    return s_fusionInstance;
}

GestureFusion::~GestureFusion() { shutdown(); }

// ── 初始化 ────────────────────────────────────────────────────────────

bool GestureFusion::initialize(const std::string& cloudEndpointId,
                               const std::string& cloudApiKey) {
    if (_initialized) return true;

    // 启动本地识别器
    auto* local = LocalGestureRecognizer::getInstance();
    local->setCallback([this](const GestureResult& r) { onLocalResult(r); });
    if (!local->start()) {
        CCLOG("[GestureFusion] Local recognizer failed to start");
        return false;
    }

    // 配置并启动云端识别器
    auto* cloud = CloudGestureRecognizer::getInstance();
    cloud->setEndpointId(cloudEndpointId);
    cloud->setApiKey(cloudApiKey);
    cloud->setCallback([this](const GestureResult& r) { onCloudResult(r); });
    if (cloud->isEnabled()) {
        cloud->start();
        CCLOG("[GestureFusion] Cloud recognizer enabled");
    } else {
        CCLOG("[GestureFusion] Cloud recognizer disabled — running local-only");
    }

    _initialized = true;
    CCLOG("[GestureFusion] Initialized — dual-channel hybrid architecture");
    return true;
}

void GestureFusion::shutdown() {
    if (!_initialized) return;
    LocalGestureRecognizer::getInstance()->stop();
    CloudGestureRecognizer::getInstance()->stop();
    _initialized = false;
    CCLOG("[GestureFusion] Shutdown");
}

// ── 本地结果回调（高频 ~30Hz）────────────────────────────────────────

void GestureFusion::onLocalResult(const GestureResult& result) {
    {
        std::lock_guard<std::mutex> lock(_localMutex);
        _latestLocal = result;
    }

    // FIST 锁定 → 触发云端校验
    if (result.gesture == GestureType::FIST && result.isLocked) {
        bool shouldSend = false;
        {
            std::lock_guard<std::mutex> lock(_pendingMutex);
            if (!_fistPendingCloud) {
                _fistPendingCloud = true;
                _fistPendingTime = std::chrono::steady_clock::now();
                shouldSend = true;
            }
        }
        if (shouldSend) {
            // 获取最新帧发送给云端
            auto* gc = GestureClient::getInstance();
            auto data = gc->getData();
            if (!data.jpegFrame.empty()) {
                CloudGestureRecognizer::getInstance()->pushFrame(data.jpegFrame);
                CCLOG("[GestureFusion] FIST locked → sending frame to cloud for verification");
            }
        }
    }

    // FIST 解除 → 重置待确认状态
    if (result.gesture != GestureType::FIST) {
        std::lock_guard<std::mutex> lock(_pendingMutex);
        _fistPendingCloud = false;
    }
}

// ── 云端结果回调（低频，仅 FIST 锁定时触发）─────────────────────────

void GestureFusion::onCloudResult(const GestureResult& result) {
    {
        std::lock_guard<std::mutex> lock(_cloudMutex);
        _latestCloud = result;
    }
    CCLOG("[GestureFusion] Cloud response: gesture=%d confidence=%.2f",
          (int)result.gesture, result.confidence);
}

// ── 每帧 tick ─────────────────────────────────────────────────────────

GestureCommand GestureFusion::tick(float dt) {
    GestureCommand cmd;

    if (!_initialized) return cmd;

    // 冷却计时
    if (_releaseCooldown > 0) _releaseCooldown -= dt;

    GestureResult local;
    {
        std::lock_guard<std::mutex> lock(_localMutex);
        local = _latestLocal;
    }

    // ── 角度输出：EMA 游戏级平滑 ──
    if (local.isValid && local.isStable) {
        // 策略3延伸：游戏层再做一次低通滤波（系数 0.7）
        _gameAngle = 0.7f * _gameAngle + 0.3f * local.angle;
    }
    cmd.targetAngle = _gameAngle;

    // ── 钩子释放 / 炸药引爆判断 ──
    // 短握拳（< 1.5s）→ 释放钩子；长握拳（≥ 1.5s）→ 引爆炸药
    bool fistNow = (local.gesture == GestureType::FIST && local.isLocked);

    if (fistNow && !_fistHeldLast) {
        // FIST 刚启动，开始计时
        _fistHoldTime = 0.0f;
    } else if (fistNow) {
        // FIST 持续中
        _fistHoldTime += dt;
    } else if (!fistNow && _fistHeldLast) {
        // FIST 结束（手张开了）
        if (_fistHoldTime > 0.1f && _fistHoldTime < BOMB_HOLD_DURATION && _releaseCooldown <= 0) {
            // 短按 → 释放钩子
            GestureResult cloud;
            bool cloudOk = false;
            {
                std::lock_guard<std::mutex> lock(_cloudMutex);
                cloud = _latestCloud;
                cloudOk = (cloud.source == "cloud" && cloud.confidence > 0.85f
                           && cloud.gesture == GestureType::FIST);
            }
            if (cloudOk || !isCloudEnabled()) {
                cmd.shouldReleaseHook = true;
                cmd.isValid = true;
                _releaseCooldown = COOLDOWN_DURATION;
                CCLOG("[GestureFusion] Short FIST (%.2fs) → RELEASE HOOK", _fistHoldTime);
            } else if (isCloudEnabled()) {
                CCLOG("[GestureFusion] Short FIST but cloud not ready → SKIP");
            }
        }
        _fistHoldTime = 0.0f;
    }

    // 长按阈值检查（FIST 持续中，每帧检测）
    if (fistNow && _fistHoldTime >= BOMB_HOLD_DURATION && _releaseCooldown <= 0) {
        cmd.shouldDetonateBomb = true;
        cmd.isValid = true;
        _fistHoldTime = 0.0f;
        _releaseCooldown = COOLDOWN_DURATION;
        CCLOG("[GestureFusion] Long FIST (>=%.1fs) → DETONATE BOMB", BOMB_HOLD_DURATION);
        std::lock_guard<std::mutex> lock(_pendingMutex);
        _fistPendingCloud = false;
        if (_cmdCallback) _cmdCallback(cmd);
    }

    _fistHeldLast = fistNow;

    if (!fistNow) {
        cmd.isValid = local.isValid && local.isStable;
    }

    return cmd;
}

// ── 预览帧 ────────────────────────────────────────────────────────────

void GestureFusion::pushPreviewFrame(const std::vector<uint8_t>& jpegData) {
    // 仅在 FIST 待确认时转发给云端（节省 API 调用）
    if (_fistPendingCloud) {
        CloudGestureRecognizer::getInstance()->pushFrame(jpegData);
    }
}

// ── 状态查询 ──────────────────────────────────────────────────────────

bool GestureFusion::isLocalRunning() const {
    return LocalGestureRecognizer::getInstance()->isRunning();
}

bool GestureFusion::isCloudEnabled() const {
    return CloudGestureRecognizer::getInstance()->isEnabled();
}

std::string GestureFusion::statusString() const {
    GestureResult local;
    {
        std::lock_guard<std::mutex> lock(_localMutex);
        local = _latestLocal;
    }
    bool pending = _fistPendingCloud;
    char buf[128];
    snprintf(buf, sizeof(buf),
             "Local: %s | Angle: %.1f | Cloud: %s | Pending: %s",
             local.gesture == GestureType::OPEN_PALM ? "OPEN" :
             local.gesture == GestureType::FIST ? "FIST" : "NONE",
             _gameAngle,
             isCloudEnabled() ? "ON" : "OFF",
             pending ? "YES" : "NO");
    return buf;
}
