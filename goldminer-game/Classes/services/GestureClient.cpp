#include "GestureClient.hpp"
#include <curl/curl.h>
#include <windows.h>
#include <cstdio>
#include <sstream>
#include "json/rapidjson.h"
#include "json/document-wrapper.h"

USING_NS_CC;

static GestureClient* s_gestureClient = nullptr;

GestureClient* GestureClient::getInstance() {
    if (!s_gestureClient) s_gestureClient = new GestureClient();
    return s_gestureClient;
}

GestureClient::~GestureClient() {
    disconnect();
}

// ── Server Launchers ──────────────────────────────────────────────────

bool GestureClient::launchServer() {
    // Launch GestureServer.exe (C++ x64 OpenCV version)
    std::string exePath = FileUtils::getInstance()->fullPathForFilename("GestureServer.exe");
    if (exePath.empty()) {
        char buf[MAX_PATH];
        GetModuleFileNameA(nullptr, buf, MAX_PATH);
        std::string dir(buf);
        dir = dir.substr(0, dir.find_last_of("\\/") + 1);
        exePath = dir + "GestureServer.exe";
    }

    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessA(exePath.c_str(), nullptr, nullptr, nullptr,
                         FALSE, 0, nullptr, nullptr, &si, &pi)) {
        CCLOG("GestureClient: Cannot launch GestureServer.exe from %s", exePath.c_str());
        return false;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

bool GestureClient::launchPythonServer() {
    // Launch python server.py (Flask + MediaPipe) in a new console window
    std::string scriptPath;

    // Try to find server.py next to the game executable
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string dir(buf);
    dir = dir.substr(0, dir.find_last_of("\\/") + 1);
    scriptPath = dir + "GestureServer/server.py";

    // Check if file exists
    DWORD attrs = GetFileAttributesA(scriptPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        // Try relative to project root
        scriptPath = dir + "../GestureServer/server.py";
        attrs = GetFileAttributesA(scriptPath.c_str());
    }
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        CCLOG("GestureClient: Cannot find server.py");
        return false;
    }

    std::string cmd = "python \"" + scriptPath + "\"";
    STARTUPINFOA si = {sizeof(si)};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWMINNOACTIVE;  // show console minimized
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessA(nullptr, const_cast<char*>(cmd.c_str()),
                         nullptr, nullptr, FALSE,
                         CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi)) {
        CCLOG("GestureClient: Cannot launch python server.py: %s", cmd.c_str());
        return false;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CCLOG("GestureClient: Python server launched — %s", scriptPath.c_str());
    return true;
}

// ── Disconnect ────────────────────────────────────────────────────────

void GestureClient::disconnect() {
    disconnectPipe();
    disconnectHttp();
}

// ── Pipe Mode ─────────────────────────────────────────────────────────

void GestureClient::connectPipe() {
    if (_running) disconnectPipe();
    _mode = GestureMode::PIPE;
    _running = true;
    _thread = std::thread(&GestureClient::pipeThreadFunc, this);
}

void GestureClient::disconnectPipe() {
    _running = false;
    _pipeOpen = false;
    if (_pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(_pipe);
        _pipe = INVALID_HANDLE_VALUE;
    }
    if (_thread.joinable()) _thread.join();
    {
        std::lock_guard<std::mutex> lock(_dataMutex);
        _data.connected = false;
    }
    _mode = GestureMode::NONE;
}

void GestureClient::pipeThreadFunc() {
    while (_running) {
        HANDLE pipe = CreateFileA(
            "\\\\.\\pipe\\GoldMinerGesture",
            GENERIC_READ,
            0, nullptr, OPEN_EXISTING, 0, nullptr);

        if (pipe != INVALID_HANDLE_VALUE) {
            _pipe = pipe;
            _pipeOpen = true;
            {
                std::lock_guard<std::mutex> lock(_dataMutex);
                _data.connected = true;
            }
            CCLOG("GestureClient: pipe connected");

            char lineBuf[256];
            int linePos = 0;

            while (_running && _pipeOpen) {
                char ch;
                DWORD bytesRead = 0;
                if (!ReadFile(pipe, &ch, 1, &bytesRead, nullptr) || bytesRead == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    break;
                }

                if (ch == '\n') {
                    lineBuf[linePos] = '\0';
                    linePos = 0;

                    float angle;
                    char gesture[32];
                    int frameSize;

                    if (sscanf(lineBuf, "ANGLE %f", &angle) == 1) {
                        std::lock_guard<std::mutex> lock(_dataMutex);
                        _data.angle = angle;
                    } else if (sscanf(lineBuf, "GESTURE %31s", gesture) == 1) {
                        Director::getInstance()->getScheduler()->performFunctionInCocosThread(
                            [this, g = std::string(gesture)]() {
                                if (_callback) {
                                    GestureData d;
                                    d.gesture = g;
                                    d.angle = _data.angle;
                                    _callback(d);
                                }
                            }
                        );
                    } else if (sscanf(lineBuf, "FRAME %d", &frameSize) == 1) {
                        std::vector<unsigned char> jpeg(frameSize);
                        DWORD totalRead = 0;
                        while (totalRead < (DWORD)frameSize) {
                            DWORD chunk;
                            if (!ReadFile(pipe, jpeg.data() + totalRead,
                                         frameSize - totalRead, &chunk, nullptr) || chunk == 0)
                                break;
                            totalRead += chunk;
                        }
                        if (totalRead == (DWORD)frameSize) {
                            std::lock_guard<std::mutex> lock(_dataMutex);
                            _data.jpegFrame = std::move(jpeg);
                            _data.hasNewFrame = true;
                        }
                    }
                } else {
                    if (linePos < 255) lineBuf[linePos++] = ch;
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(_dataMutex);
            _data.connected = false;
        }
        _pipeOpen = false;
        if (_pipe != INVALID_HANDLE_VALUE) {
            CloseHandle(_pipe);
            _pipe = INVALID_HANDLE_VALUE;
        }

        if (_running) std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

// ── HTTP Mode ─────────────────────────────────────────────────────────

size_t GestureClient::httpWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* str = static_cast<std::string*>(userdata);
    str->append(ptr, size * nmemb);
    return size * nmemb;
}

void GestureClient::connectHttp(const std::string& url) {
    if (_running) disconnect();

    _mode = GestureMode::HTTP;
    _running = true;
    _thread = std::thread(&GestureClient::httpThreadFunc, this, url);
}

void GestureClient::disconnectHttp() {
    _running = false;
    if (_thread.joinable()) _thread.join();
    {
        std::lock_guard<std::mutex> lock(_dataMutex);
        _data.connected = false;
    }
    _mode = GestureMode::NONE;
}

void GestureClient::httpThreadFunc(const std::string& baseUrl) {
    // Wait for GestureServer.exe to start up
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    const std::string gestureUrl = baseUrl + "/gesture";
    const std::string frameUrl = baseUrl + "/frame";

    std::string lastGesture;
    int stableCount = 0;

    while (_running) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // ── Poll gesture API ──
        std::string gestureResp;
        curl_easy_setopt(curl, CURLOPT_URL, gestureUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, httpWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &gestureResp);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 200L);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OK && !gestureResp.empty()) {
            rapidjson::Document doc;
            doc.Parse(gestureResp.c_str());
            if (!doc.HasParseError()) {
                float angle = 0.0f;
                std::string gesture;

                if (doc.HasMember("angle")) angle = doc["angle"].GetFloat();
                if (doc.HasMember("gesture")) gesture = doc["gesture"].GetString();
                bool connected = doc.HasMember("connected") ? doc["connected"].GetBool() : false;

                {
                    std::lock_guard<std::mutex> lock(_dataMutex);
                    _data.angle = angle;
                    _data.connected = connected;
                }

                // Stability filter
                if (gesture == lastGesture) {
                    stableCount++;
                } else {
                    stableCount = 0;
                    lastGesture = gesture;
                }

                if (stableCount >= 3 && !gesture.empty() && gesture != "NONE") {
                    std::lock_guard<std::mutex> lock(_dataMutex);
                    _data.gesture = gesture;
                }

                // Fire callback on cocos thread for FIST (握拳 = drop hook)
                if (gesture == "FIST" && stableCount == 3) {
                    Director::getInstance()->getScheduler()->performFunctionInCocosThread(
                        [this]() {
                            if (_callback) {
                                GestureData d;
                                {
                                    std::lock_guard<std::mutex> lock(_dataMutex);
                                    d = _data;
                                }
                                _callback(d);
                            }
                        }
                    );
                }
            }
        }

        // ── Poll frame API ──
        std::string frameResp;
        curl_easy_setopt(curl, CURLOPT_URL, frameUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, httpWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &frameResp);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 200L);

        res = curl_easy_perform(curl);
        if (res == CURLE_OK && !frameResp.empty()) {
            std::lock_guard<std::mutex> lock(_dataMutex);
            _data.jpegFrame.assign(frameResp.begin(), frameResp.end());
            _data.hasNewFrame = true;
        }

        curl_easy_cleanup(curl);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// ── Get Latest Data ───────────────────────────────────────────────────

GestureData GestureClient::getData() {
    std::lock_guard<std::mutex> lock(_dataMutex);
    GestureData copy = _data;
    _data.hasNewFrame = false;
    return copy;
}
