#include "AIGestureService.hpp"
#include <curl/curl.h>
#include "json/rapidjson.h"
#include "json/document-wrapper.h"
#include "json/writer.h"
#include "json/stringbuffer.h"
#include <sstream>

USING_NS_CC;

static AIGestureService* s_aiInstance = nullptr;

AIGestureService* AIGestureService::getInstance() {
    if (!s_aiInstance) {
        s_aiInstance = new AIGestureService();
    }
    return s_aiInstance;
}

AIGestureService::~AIGestureService() = default;

std::string AIGestureService::encodeBase64(const std::vector<unsigned char>& data) {
    static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
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

std::string AIGestureService::buildRequestBody(const std::string& imageB64, const std::string& model) {
    rapidjson::Document doc;
    doc.SetObject();
    auto& alloc = doc.GetAllocator();

    doc.AddMember("model", rapidjson::Value(model.c_str(), alloc), alloc);
    doc.AddMember("temperature", 0.0, alloc);

    // Force JSON output (supported by doubao / OpenAI-compatible APIs)
    rapidjson::Value responseFormat(rapidjson::kObjectType);
    responseFormat.AddMember("type", "json_object", alloc);
    doc.AddMember("response_format", responseFormat, alloc);

    rapidjson::Value messages(rapidjson::kArrayType);

    // System prompt — higher compliance than user prompt alone
    rapidjson::Value sysMsg(rapidjson::kObjectType);
    sysMsg.AddMember("role", "system", alloc);
    sysMsg.AddMember("content", "You are a gesture recognizer. Always output strictly valid JSON without markdown.", alloc);
    messages.PushBack(sysMsg, alloc);

    // User message with image
    rapidjson::Value userMsg(rapidjson::kObjectType);
    userMsg.AddMember("role", "user", alloc);

    rapidjson::Value contentArr(rapidjson::kArrayType);

    rapidjson::Value textPart(rapidjson::kObjectType);
    textPart.AddMember("type", "text", alloc);
    textPart.AddMember("text", rapidjson::Value(
        "Detect the clearest hand. Return JSON: {\"gesture\": \"open_palm\"|\"fist\"|\"unknown\", \"confidence\": float(0-1)}",
        alloc), alloc);
    contentArr.PushBack(textPart, alloc);

    rapidjson::Value imagePart(rapidjson::kObjectType);
    imagePart.AddMember("type", "image_url", alloc);
    rapidjson::Value imageUrl(rapidjson::kObjectType);
    std::string url = "data:image/jpeg;base64," + imageB64;
    imageUrl.AddMember("url", rapidjson::Value(url.c_str(), alloc), alloc);
    imageUrl.AddMember("detail", "low", alloc);  // faster inference, sufficient for gesture
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

size_t AIGestureService::writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* str = static_cast<std::string*>(userdata);
    str->append(ptr, size * nmemb);
    return size * nmemb;
}

void AIGestureService::sendFrame(const std::vector<unsigned char>& jpegData) {
    if (jpegData.empty()) return;
    if (!isEnabled()) {
        static bool warned = false;
        if (!warned) {
            CCLOG("[AI] AIGestureService disabled — endpointId or apiKey not set.");
            warned = true;
        }
        return;
    }

    // Atomic CAS — drop frame if a request is already in flight
    if (_requestInFlight.exchange(true)) return;

    std::string apiKey = _apiKey;
    std::string apiUrl = _apiUrl;
    std::string endpointId = _endpointId;
    std::vector<unsigned char> dataCopy = jpegData;

    std::thread([this, dataCopy = std::move(dataCopy), apiKey = std::move(apiKey),
                 apiUrl = std::move(apiUrl), endpointId = std::move(endpointId)]() {

        // RAII guard: _requestInFlight is ALWAYS reset when this thread exits,
        // regardless of exceptions, early returns, or curl failures.
        std::shared_ptr<void> guard(nullptr, [this](void*) {
            _requestInFlight = false;
        });

        // Encode on worker thread — frees Cocos render thread from heavy CPU work
        std::string imageB64 = encodeBase64(dataCopy);
        std::string body = buildRequestBody(imageB64, endpointId);

        CCLOG("[AI] Sending frame to %s (image=%d bytes b64)",
              apiUrl.c_str(), (int)imageB64.size());

        std::string response;
        CURL* curl = curl_easy_init();

        if (!curl) {
            CCLOG("[AI] ERROR: curl_easy_init() returned NULL");
            return;
        }

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, ("Authorization: Bearer " + apiKey).c_str());

        curl_easy_setopt(curl, CURLOPT_URL, apiUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);  // 2s — older commands are useless in-game

        CURLcode res = curl_easy_perform(curl);

        long httpCode = 0;
        if (res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
            CCLOG("[AI] HTTP %ld, response size=%d bytes", httpCode, (int)response.size());
        } else {
            CCLOG("[AI] ERROR: curl_easy_perform failed: %s", curl_easy_strerror(res));
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK && httpCode == 200 && !response.empty()) {
            Director::getInstance()->getScheduler()->performFunctionInCocosThread(
                [this, response = std::move(response)]() {
                    rapidjson::Document doc;
                    doc.Parse(response.c_str());
                    if (doc.HasParseError() || !doc.HasMember("choices")) {
                        CCLOG("[AI] Outer JSON Parse Error. Response: %s", response.c_str());
                        return;
                    }

                    const auto& choices = doc["choices"];
                    if (!choices.IsArray() || choices.Size() == 0) return;

                    std::string content = choices[0]["message"]["content"].GetString();

                    // Strip markdown code fences if model wraps JSON in ```json ... ```
                    size_t start = content.find('{');
                    size_t end = content.rfind('}');
                    if (start != std::string::npos && end != std::string::npos && end > start) {
                        content = content.substr(start, end - start + 1);
                    }

                    rapidjson::Document inner;
                    inner.Parse(content.c_str());
                    if (inner.HasParseError() || !inner.HasMember("gesture")) {
                        CCLOG("[AI] Inner JSON Parse Error. Content: %s", content.c_str());
                        return;
                    }

                    std::string gesture = inner["gesture"].GetString();
                    float confidence = inner.HasMember("confidence") ? inner["confidence"].GetFloat() : 1.0f;
                    CCLOG("[AI] Gesture=%s confidence=%.2f", gesture.c_str(), confidence);
                    if (_callback) _callback(gesture, confidence);
                }
            );
        } else if (httpCode != 200 && !response.empty()) {
            CCLOG("[AI] ERROR response HTTP %ld: %s", httpCode, response.c_str());
        }
    }).detach();
}
