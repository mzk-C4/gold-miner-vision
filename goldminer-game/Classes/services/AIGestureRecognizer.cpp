#include "AIGestureRecognizer.hpp"
#include "GestureClient.hpp"
#include "cocos2d.h"
#include <curl/curl.h>
#include "json/rapidjson.h"
#include "json/document-wrapper.h"
#include "json/writer.h"
#include "json/stringbuffer.h"
#include <chrono>

USING_NS_CC;

static AIGestureRecognizer* s_aiInstance = nullptr;

AIGestureRecognizer* AIGestureRecognizer::getInstance() {
    if (!s_aiInstance) s_aiInstance = new AIGestureRecognizer();
    return s_aiInstance;
}

AIGestureRecognizer::~AIGestureRecognizer() { stop(); }

// ── Base64 编码 ─────────────────────────────────────────────────────────

static std::string encodeB64(const std::vector<uint8_t>& data) {
    static const char* chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve(((data.size() + 2) / 3) * 4);
    for (size_t i = 0; i < data.size(); i += 3) {
        unsigned int n = (unsigned char)data[i] << 16;
        if (i + 1 < data.size()) n |= (unsigned char)data[i + 1] << 8;
        if (i + 2 < data.size()) n |= (unsigned char)data[i + 2];
        result.push_back(chars[(n >> 18) & 0x3F]);
        result.push_back(chars[(n >> 12) & 0x3F]);
        result.push_back(i + 1 < data.size() ? chars[(n >> 6) & 0x3F] : '=');
        result.push_back(i + 2 < data.size() ? chars[n & 0x3F] : '=');
    }
    return result;
}

// ── 构建 API 请求体（增强提示词：手势分类 + 手部水平位置）─────────────

static std::string buildBody(const std::string& imageB64, const std::string& model) {
    rapidjson::Document doc;
    doc.SetObject();
    auto& alloc = doc.GetAllocator();

    doc.AddMember("model", rapidjson::Value(model.c_str(), alloc), alloc);
    doc.AddMember("temperature", 0.0, alloc);

    rapidjson::Value responseFormat(rapidjson::kObjectType);
    responseFormat.AddMember("type", "json_object", alloc);
    doc.AddMember("response_format", responseFormat, alloc);

    rapidjson::Value messages(rapidjson::kArrayType);

    // System prompt
    rapidjson::Value sysMsg(rapidjson::kObjectType);
    sysMsg.AddMember("role", "system", alloc);
    sysMsg.AddMember("content",
        "You are a high-precision hand gesture analyzer for a real-time game. "
        "Examine the image and determine:\n"
        "1. GESTURE: \"open_palm\" (all 5 fingers extended), "
        "\"fist\" (fingers curled tight), "
        "or \"unknown\" (no clear hand / ambiguous).\n"
        "2. POSITION: Horizontal position of the hand/wrist center, normalized: "
        "-1.0 = left edge, +1.0 = right edge, 0.0 = center. Be precise.\n"
        "Output STRICT JSON only, no markdown.",
        alloc);
    messages.PushBack(sysMsg, alloc);

    // User message with image
    rapidjson::Value userMsg(rapidjson::kObjectType);
    userMsg.AddMember("role", "user", alloc);

    rapidjson::Value contentArr(rapidjson::kArrayType);

    rapidjson::Value textPart(rapidjson::kObjectType);
    textPart.AddMember("type", "text", alloc);
    textPart.AddMember("text", rapidjson::Value(
        "仔细观察图片中最清晰的一只手，判断手势和位置：\n"
        "1. GESTURE 手势：\n"
        "   - \"open_palm\": 手指完全伸展，5指清晰可见\n"
        "   - \"fist\": 手指紧握成拳，看不到伸展的手指\n"
        "   - \"unknown\": 无清晰手部或无法判断\n"
        "2. POSITION 水平位置：手部/手腕中心在画面中的归一化位置，"
        "-1.0=最左边, +1.0=最右边, 0.0=正中央。请精确估计。\n"
        "严格返回 JSON："
        "{\"gesture\":\"...\", \"position\":0.0, \"confidence\":0.0}",
        alloc), alloc);
    contentArr.PushBack(textPart, alloc);

    rapidjson::Value imagePart(rapidjson::kObjectType);
    imagePart.AddMember("type", "image_url", alloc);
    rapidjson::Value imageUrl(rapidjson::kObjectType);
    std::string url = "data:image/jpeg;base64," + imageB64;
    imageUrl.AddMember("url", rapidjson::Value(url.c_str(), alloc), alloc);
    imageUrl.AddMember("detail", "low", alloc);
    imagePart.AddMember("image_url", imageUrl, alloc);
    contentArr.PushBack(imagePart, alloc);

    userMsg.AddMember("content", contentArr, alloc);
    messages.PushBack(userMsg, alloc);
    doc.AddMember("messages", messages, alloc);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    return buffer.GetString();
}

static size_t writeCb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* str = static_cast<std::string*>(userdata);
    str->append(ptr, size * nmemb);
    return size * nmemb;
}

// ── 生命周期 ────────────────────────────────────────────────────────────

bool AIGestureRecognizer::start() {
    if (_running) return true;
    if (!isEnabled()) {
        CCLOG("[AIGesture] Disabled — endpointId or apiKey not set");
        return false;
    }

    // 启动摄像头进程（仅画面采集，OpenCV 仅做 I/O，不做手势检测）
    auto* gc = GestureClient::getInstance();
    if (!gc->launchServer()) {
        CCLOG("[AIGesture] Failed to launch GestureServer (camera)");
        return false;
    }
    // 不调用 connectHttp — 本识别器自行轮询 /frame 端点

    _running = true;
    _thread = std::thread(&AIGestureRecognizer::inferenceLoop, this);
    CCLOG("[AIGesture] Started — camera(OpenCV I/O only) -> AI vision pipeline");
    return true;
}

void AIGestureRecognizer::stop() {
    _running = false;
    if (_thread.joinable()) _thread.join();
    GestureClient::getInstance()->disconnect();
    CCLOG("[AIGesture] Stopped");
}

void AIGestureRecognizer::pushFrame(const std::vector<uint8_t>& jpegData) {
    if (jpegData.empty()) return;
    std::lock_guard<std::mutex> lock(_frameMutex);
    _latestFrame = jpegData;
    _hasFrame = true;
}

std::vector<uint8_t> AIGestureRecognizer::getLatestFrame() {
    std::lock_guard<std::mutex> lock(_frameMutex);
    if (_hasFrame) {
        _hasFrame = false;
        return _latestFrame;
    }
    return {};
}

// ── 推理主循环 ──────────────────────────────────────────────────────────

void AIGestureRecognizer::inferenceLoop() {
    CCLOG("[AIGesture] Inference thread starting...");

    // 等待 GestureServer 启动
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    const std::string frameUrl = "http://localhost:5000/frame";

    while (_running) {
        // 从 GestureServer 获取最新 JPEG 帧（仅采集，非手势识别）
        std::string frameResp;
        CURL* curl = curl_easy_init();
        if (!curl) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        curl_easy_setopt(curl, CURLOPT_URL, frameUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &frameResp);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 500L);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK || frameResp.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        std::vector<uint8_t> jpeg(frameResp.begin(), frameResp.end());

        // 存储最新帧供游戏预览
        {
            std::lock_guard<std::mutex> lock(_frameMutex);
            _latestFrame = jpeg;
            _hasFrame = true;
        }

        // Base64 编码
        std::string imageB64 = encodeB64(jpeg);

        // 构建请求体
        std::string body = buildBody(imageB64, _endpointId);

        // 发送给 AI 视觉模型
        std::string response;
        curl = curl_easy_init();
        if (!curl) {
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
            continue;
        }

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers,
            ("Authorization: Bearer " + _apiKey).c_str());

        curl_easy_setopt(curl, CURLOPT_URL, _apiUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);

        CURLcode httpRes = curl_easy_perform(curl);
        long httpCode = 0;
        if (httpRes == CURLE_OK)
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (httpRes == CURLE_OK && httpCode == 200 && !response.empty()) {
            Director::getInstance()->getScheduler()->performFunctionInCocosThread(
                [this, response = std::move(response)]() {
                    rapidjson::Document doc;
                    doc.Parse(response.c_str());
                    if (doc.HasParseError() || !doc.HasMember("choices")) return;

                    const auto& choices = doc["choices"];
                    if (!choices.IsArray() || choices.Size() == 0) return;

                    std::string content = choices[0]["message"]["content"].GetString();

                    size_t start = content.find('{');
                    size_t end = content.rfind('}');
                    if (start != std::string::npos && end != std::string::npos && end > start)
                        content = content.substr(start, end - start + 1);

                    rapidjson::Document inner;
                    inner.Parse(content.c_str());
                    if (inner.HasParseError() || !inner.HasMember("gesture")) {
                        CCLOG("[AIGesture] JSON parse error: %s", content.c_str());
                        return;
                    }

                    std::string gestureStr = inner["gesture"].GetString();
                    float position = inner.HasMember("position")
                        ? inner["position"].GetFloat() : 0.0f;
                    float confidence = inner.HasMember("confidence")
                        ? inner["confidence"].GetFloat() : 1.0f;

                    // 归一化位置 → 钩子角度 [-65, +65]
                    float angle = position * 65.0f;
                    if (angle < -65.0f) angle = -65.0f;
                    if (angle > 65.0f) angle = 65.0f;

                    GestureResult result;
                    result.source = "ai";
                    result.angle = angle;
                    result.confidence = confidence;

                    if (gestureStr == "open_palm") {
                        result.gesture = GestureType::OPEN_PALM;
                        result.isValid = true;
                    } else if (gestureStr == "fist") {
                        result.gesture = GestureType::FIST;
                        result.isValid = true;
                    } else {
                        result.gesture = GestureType::UNKNOWN;
                        result.isValid = false;
                    }

                    result.isStable = (confidence > 0.7f);
                    result.isLocked = (result.gesture == GestureType::FIST && confidence > 0.7f);

                    CCLOG("[AIGesture] %s | pos=%.2f angle=%.1f conf=%.2f",
                          gestureStr.c_str(), position, angle, confidence);

                    if (_callback) _callback(result);
                }
            );
        } else {
            CCLOG("[AIGesture] HTTP %ld / curl err %d", httpCode, (int)httpRes);
        }

        // 推理间隔（避免 API 限流 + 省钱）
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
    }

    CCLOG("[AIGesture] Inference thread exited");
}
