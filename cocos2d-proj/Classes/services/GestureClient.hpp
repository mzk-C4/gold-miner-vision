#pragma once

#include "cocos2d.h"
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <mutex>

struct GestureData {
    float angle = 0.0f;         // hook angle in degrees
    std::string gesture;         // "OPEN_PALM", "FIST", "NONE"
    std::vector<unsigned char> jpegFrame; // latest camera preview JPEG
    bool hasNewFrame = false;
    bool connected = false;
};

class GestureClient {
public:
    using DataCallback = std::function<void(const GestureData&)>;

    static GestureClient* getInstance();

    void connect();
    void disconnect();
    bool isConnected() const { return _data.connected; }

    /// Get latest data (thread-safe)
    GestureData getData();

    /// Set callback invoked on cocos thread when new data arrives
    void setCallback(DataCallback cb) { _callback = std::move(cb); }

    /// Launch the external GestureServer.exe
    bool launchServer();

private:
    GestureClient() = default;
    ~GestureClient();
    void pipeThreadFunc();

    std::thread _thread;
    std::atomic<bool> _running{false};
    DataCallback _callback;

    GestureData _data;
    std::mutex _dataMutex;

    HANDLE _pipe = INVALID_HANDLE_VALUE;
    std::atomic<bool> _pipeOpen{false};
};
