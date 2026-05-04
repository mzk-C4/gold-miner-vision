#pragma once
#include <string>

/// 手势类型：仅三种状态，覆盖游戏全部交互
enum class GestureType {
    OPEN_PALM,  // 张开手掌 → 瞄准模式：钩子连续摆动
    FIST,       // 握拳     → 释放钩子
    UNKNOWN     // 无手势   → 维持当前状态
};

/// 原始识别结果（由识别器产生）
struct GestureResult {
    GestureType gesture = GestureType::UNKNOWN;
    float angle       = 0.0f;    // 钩子角度 [-65, +65]，左负右正
    float confidence  = 0.0f;    // 置信度 [0, 1]
    bool  isValid     = false;   // 结果是否有效（未检测到人手时为 false）
    bool  isStable    = false;   // 是否通过时域去抖（连续 N 帧一致）
    bool  isLocked    = false;   // 是否触发"状态锁定"（FIST 连续 3 帧）
    std::string source;          // "local" / "cloud" / "fusion" — 调试溯源
};

/// 高层指令（由融合器产生，发往游戏逻辑）
struct GestureCommand {
    bool  shouldReleaseHook   = false;  // 本帧是否释放钩子
    bool  shouldDetonateBomb  = false;  // 本帧是否引爆炸药（长时间握拳）
    float targetAngle         = 0.0f;   // 钩子目标角度
    bool  isValid             = false;  // 指令是否有效（有人手在画面中）
};
