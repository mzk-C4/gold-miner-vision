#include "GestureClient.hpp"
#include <windows.h>
#include <cstdio>

USING_NS_CC;

static GestureClient* s_gestureClient = nullptr;

GestureClient* GestureClient::getInstance() {
    if (!s_gestureClient) s_gestureClient = new GestureClient();
    return s_gestureClient;
}

GestureClient::~GestureClient() {
    disconnect();
}

bool GestureClient::launchServer() {
    // Try to start GestureServer.exe next to the game executable
    std::string exePath = FileUtils::getInstance()->fullPathForFilename("GestureServer.exe");
    if (exePath.empty()) {
        // Try relative to the game bin directory
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

void GestureClient::connect() {
    if (_running) return;
    _running = true;
    _thread = std::thread(&GestureClient::pipeThreadFunc, this);
}

void GestureClient::disconnect() {
    _running = false;
    _pipeOpen = false;
    if (_pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(_pipe);
        _pipe = INVALID_HANDLE_VALUE;
    }
    if (_thread.joinable()) _thread.join();
}

GestureData GestureClient::getData() {
    std::lock_guard<std::mutex> lock(_dataMutex);
    GestureData copy = _data;
    _data.hasNewFrame = false;
    return copy;
}

void GestureClient::pipeThreadFunc() {
    // Wait and retry connection
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
            CCLOG("GestureClient: connected to pipe");

            // Set to blocking reads with timeout via overlapped
            char lineBuf[256];
            int linePos = 0;

            while (_running && _pipeOpen) {
                char ch;
                DWORD bytesRead = 0;
                if (!ReadFile(pipe, &ch, 1, &bytesRead, nullptr) || bytesRead == 0) {
                    // Pipe broken, retry
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
                        // Read binary JPEG data
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
                    // Ignore HEARTBEAT and unknown lines
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
