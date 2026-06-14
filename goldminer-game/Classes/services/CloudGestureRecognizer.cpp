#include "CloudGestureRecognizer.hpp"
#include "cocos2d.h"
#include <curl/curl.h>
#include "json/rapidjson.h"
#include "json/document-wrapper.h"
#include "json/writer.h"
#include "json/stringbuffer.h"

USING_NS_CC;

static CloudGestureRecognizer* s_cloudInstance = nullptr;

CloudGestureRecognizer* CloudGestureRecognizer::getInstance() {
    if (!s_cloudInstance) s_cloudInstance = new CloudGestureRecognizer();
    return s_cloudInstance;
}

CloudGestureRecognizer::~CloudGestureRecognizer() { stop(); }

// ── Base64 编码 ───────────────────────────────────────────────────────

static std::string encodeB64(const std::vector<uint8_t>& data) {
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

// ── 策略5：增强提示词 ─────────────────────────────────────────────────

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

    // System prompt — 给模型的角色与输出约束
    rapidjson::Value sysMsg(rapidjson::kObjectType);
    sysMsg.AddMember("role", "system", alloc);
    sysMsg.AddMember("content",
        "You are a high-precision gesture classifier. Your task is to examine the "
        "clearest hand in the image and determine its state. You MUST follow these rules:\n"
        "- `open_palm`: fingers are fully extended, 5 fingers clearly visible.\n"
        "- `fist`: fingers are curled into a tight fist, no extended fingers visible.\n"
        "- `unknown`: no hand in the image, or the gesture is ambiguous.\n"
        "Always output strictly valid JSON without markdown formatting.",
        alloc);
    messages.PushBack(sysMsg, alloc);

    // User message with image
    rapidjson::Value userMsg(rapidjson::kObjectType);
    userMsg.AddMember("role", "user", alloc);

    rapidjson::Value contentArr(rapidjson::kArrayType);

    rapidjson::Value textPart(rapidjson::kObjectType);
    textPart.AddMember("type", "text", alloc);
    textPart.AddMember("text", rapidjson::Value(
        "请作为高精度手势分类器。仔细观察图片中最清晰的一只手，判断其状态。"
        "必须严格遵守以下规则：\n"
        "- `open_palm`: 手指完全伸展，能清晰看到五根手指。\n"
        "- `fist`: 手指紧握成拳，看不到伸展的手指。\n"
        "- `unknown`: 图片中没有手，或手势介于两者之间，无法确定。\n"
        "请只返回一个JSON对象，格式为: "
        "{\"gesture\": \"open_palm or fist or unknown\", \"confidence\": 0.0到1.0之间的数字}",
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

// ── 生命周期 ──────────────────────────────────────────────────────────

bool CloudGestureRecognizer::start() {
    if (_running) return true;
    if (!isEnabled()) {
        CCLOG("[CloudRecognizer] Disabled — endpointId or apiKey not set");
        return false;
    }
    _running = true;
    CCLOG("[CloudRecognizer] Started");
    return true;
}

void CloudGestureRecognizer::stop() {
    _running = false;
    _requestInFlight = false;
    CCLOG("[CloudRecognizer] Stopped");
}

// ── 异步帧发送 ────────────────────────────────────────────────────────

void CloudGestureRecognizer::pushFrame(const std::vector<uint8_t>& jpegData) {
    if (jpegData.empty() || !_running || !isEnabled()) return;
    if (_requestInFlight.exchange(true)) return;  // CAS 防重入

    std::string apiKey = _apiKey;
    std::string apiUrl = _apiUrl;
    std::string endpointId = _endpointId;
    std::vector<uint8_t> dataCopy = jpegData;

    std::thread([this, dataCopy = std::move(dataCopy), apiKey = std::move(apiKey),
                 apiUrl = std::move(apiUrl), endpointId = std::move(endpointId)]() {

        // RAII guard — 无论何种退出路径，_requestInFlight 必定复位
        std::shared_ptr<void> guard(nullptr, [this](void*) {
            _requestInFlight = false;
        });

        std::string imageB64 = encodeB64(dataCopy);
        std::string body = buildBody(imageB64, endpointId);

        CCLOG("[CloudRecognizer] Sending frame (%d bytes JPEG, %d bytes b64)",
              (int)dataCopy.size(), (int)imageB64.size());

        std::string response;
        CURL* curl = curl_easy_init();
        if (!curl) {
            CCLOG("[CloudRecognizer] ERROR: curl_easy_init() returned NULL");
            return;
        }

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, ("Authorization: Bearer " + apiKey).c_str());

        curl_easy_setopt(curl, CURLOPT_URL, apiUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);

        CURLcode res = curl_easy_perform(curl);
        long httpCode = 0;
        if (res == CURLE_OK)
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK && httpCode == 200 && !response.empty()) {
            Director::getInstance()->getScheduler()->performFunctionInCocosThread(
                [this, response = std::move(response)]() {
                    rapidjson::Document doc;
                    doc.Parse(response.c_str());
                    if (doc.HasParseError() || !doc.HasMember("choices")) {
                        CCLOG("[CloudRecognizer] Outer JSON parse error");
                        return;
                    }

                    const auto& choices = doc["choices"];
                    if (!choices.IsArray() || choices.Size() == 0) return;

                    std::string content = choices[0]["message"]["content"].GetString();

                    // Strip markdown code fences
                    size_t start = content.find('{');
                    size_t end = content.rfind('}');
                    if (start != std::string::npos && end != std::string::npos && end > start)
                        content = content.substr(start, end - start + 1);

                    rapidjson::Document inner;
                    inner.Parse(content.c_str());
                    if (inner.HasParseError() || !inner.HasMember("gesture")) {
                        CCLOG("[CloudRecognizer] Inner JSON parse error: %s", content.c_str());
                        return;
                    }

                    std::string gestureStr = inner["gesture"].GetString();
                    float confidence = inner.HasMember("confidence")
                        ? inner["confidence"].GetFloat() : 1.0f;

                    GestureResult result;
                    result.source = "cloud";
                    result.confidence = confidence;
                    if (gestureStr == "open_palm") result.gesture = GestureType::OPEN_PALM;
                    else if (gestureStr == "fist") result.gesture = GestureType::FIST;
                    else result.gesture = GestureType::UNKNOWN;

                    CCLOG("[CloudRecognizer] gesture=%s confidence=%.2f",
                          gestureStr.c_str(), confidence);

                    if (_callback) _callback(result);
                }
            );
        } else {
            CCLOG("[CloudRecognizer] HTTP %ld or curl error: %s",
                  httpCode, res != CURLE_OK ? curl_easy_strerror(res) : "OK");
        }
    }).detach();
}
