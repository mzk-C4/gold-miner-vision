#pragma once
#include "GestureData.hpp"
#include <functional>
#include <vector>
#include <cstdint>

/// 手势识别器抽象接口 —— 本地/云端识别器均实现此接口
/// 设计原则：
///   - 帧推送与结果回调完全异步，识别器内部自行管理线程
///   - start()/stop() 控制识别器生命周期
///   - 通过回调将 GestureResult 推送给上层（融合器）
class IGestureRecognizer {
public:
    using ResultCallback = std::function<void(const GestureResult&)>;

    virtual ~IGestureRecognizer() = default;

    /// 启动识别器（初始化资源、打开摄像头/建立连接）
    virtual bool start() = 0;

    /// 停止识别器（释放资源）
    virtual void stop() = 0;

    /// 注册结果回调（在 start 之前设置）
    virtual void setCallback(ResultCallback cb) = 0;

    /// 推送一帧 JPEG 图像用于识别（异步，立即返回）
    virtual void pushFrame(const std::vector<uint8_t>& jpegData) = 0;

    /// 识别器是否正在运行
    virtual bool isRunning() const = 0;

    /// 识别器名称（调试用）
    virtual const char* name() const = 0;
};
