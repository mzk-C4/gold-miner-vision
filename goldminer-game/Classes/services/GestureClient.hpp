#pragma once

#include "cocos2d.h"
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <mutex>

enum class GestureMode { NONE, PIPE, HTTP };

struct GestureData {
    float angle = 0.0f;
    std::string gesture;          // "OPEN_PALM", "FIST", "NONE"
    std::vector<unsigned char> jpegFrame;
    bool hasNewFrame = false;
    bool connected = false;
};

class GestureClient {
public:
    using DataCallback = std::function<void(const GestureData&)>;

    static GestureClient* getInstance();

    /// PIPE mode — connect to GestureServer.exe via named pipe
    void connectPipe();
    void disconnectPipe();

    /// HTTP mode — poll Python GestureServer (Flask + MediaPipe)
    void connectHttp(const std::string& url = "http://localhost:5000");
    void disconnectHttp();

    /// Disconnect any active mode
    void disconnect();

    bool isConnected() const { return _data.connected; }
    GestureMode getMode() const { return _mode; }

    /// Get latest gesture data (thread-safe)
    GestureData getData();

    /// Callback invoked on cocos thread when gesture/angle data arrives
    void setCallback(DataCallback cb) { _callback = std::move(cb); }

    /// Launch GestureServer.exe (pipe mode) or python server.py (HTTP mode)
    bool launchServer();
    bool launchPythonServer();

private:
    GestureClient() = default;
    ~GestureClient();

    void pipeThreadFunc();
    void httpThreadFunc(const std::string& url);
    static size_t httpWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata);

    std::thread _thread;
    std::atomic<bool> _running{false};
    DataCallback _callback;

    GestureData _data;
    std::mutex _dataMutex;
    GestureMode _mode = GestureMode::NONE;

    HANDLE _pipe = INVALID_HANDLE_VALUE;
    std::atomic<bool> _pipeOpen{false};
};
