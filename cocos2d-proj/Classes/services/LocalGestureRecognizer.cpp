#include "LocalGestureRecognizer.hpp"
#include <curl/curl.h>
#include "json/rapidjson.h"
#include "json/document-wrapper.h"
#include <chrono>

USING_NS_CC;

static LocalGestureRecognizer* s_localInstance = nullptr;

LocalGestureRecognizer* LocalGestureRecognizer::getInstance() {
    if (!s_localInstance) s_localInstance = new LocalGestureRecognizer();
    return s_localInstance;
}

LocalGestureRecognizer::~LocalGestureRecognizer() { stop(); }

static size_t localWriteCb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* str = static_cast<std::string*>(userdata);
    str->append(ptr, size * nmemb);
    return size * nmemb;
}

bool LocalGestureRecognizer::start() {
    if (_running) return true;

    auto* gc = GestureClient::getInstance();
    if (!gc->launchServer()) {
        CCLOG("[LocalRecognizer] Failed to launch GestureServer.exe");
        return false;
    }

    // 连接到 GestureServer 的 HTTP API
    gc->connectHttp("http://localhost:5000");

    _running = true;
    _thread = std::thread(&LocalGestureRecognizer::pollThread, this);
    CCLOG("[LocalRecognizer] Started — polling GestureServer");
    return true;
}

void LocalGestureRecognizer::stop() {
    _running = false;
    if (_thread.joinable()) _thread.join();
    GestureClient::getInstance()->disconnect();
    CCLOG("[LocalRecognizer] Stopped");
}

void LocalGestureRecognizer::pushFrame(const std::vector<uint8_t>&) {
    // GestureServer 直接从摄像头取帧，不需要游戏端推送
}

void LocalGestureRecognizer::pollThread() {
    // 等待 GestureServer 启动
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    const std::string url = "http://localhost:5000/gesture";

    while (_running) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        std::string resp;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, localWriteCb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 150L);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OK && !resp.empty()) {
            rapidjson::Document doc;
            doc.Parse(resp.c_str());
            if (!doc.HasParseError()) {
                GestureResult result;
                result.source = "local";

                if (doc.HasMember("angle"))
                    result.angle = doc["angle"].GetFloat();
                if (doc.HasMember("gesture")) {
                    std::string g = doc["gesture"].GetString();
                    if (g == "OPEN_PALM") result.gesture = GestureType::OPEN_PALM;
                    else if (g == "FIST") result.gesture = GestureType::FIST;
                    else result.gesture = GestureType::UNKNOWN;
                }
                if (doc.HasMember("connected"))
                    result.isValid = doc["connected"].GetBool();

                // GestureServer 已经做了 3 帧锁定，这里直接信任
                if (result.isValid && result.gesture != GestureType::UNKNOWN) {
                    result.isStable = true;
                    result.isLocked = true;

                    // 推送最新 JPEG 帧给云端通道（异步，不阻塞）
                    auto* gc = GestureClient::getInstance();
                    auto data = gc->getData();
                    if (data.hasNewFrame && !data.jpegFrame.empty()) {
                        // 帧数据由 Game::updateCameraPreview 处理
                    }
                }

                if (_callback) {
                    Director::getInstance()->getScheduler()->performFunctionInCocosThread(
                        [this, result]() { if (_callback) _callback(result); }
                    );
                }
            }
        }

        curl_easy_cleanup(curl);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
