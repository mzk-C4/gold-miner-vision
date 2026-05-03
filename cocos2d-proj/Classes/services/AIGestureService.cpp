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
    doc.AddMember("temperature", 0.1, alloc);

    rapidjson::Value messages(rapidjson::kArrayType);
    rapidjson::Value msg(rapidjson::kObjectType);
    msg.AddMember("role", "user", alloc);

    rapidjson::Value contentArr(rapidjson::kArrayType);

    rapidjson::Value textPart(rapidjson::kObjectType);
    textPart.AddMember("type", "text", alloc);
    textPart.AddMember("text", rapidjson::Value(
        "你是一个手势识别器。请观察图片中最清晰的一只手，判断手势为\"open_palm\"（张开手掌）、\"fist\"（握拳），或\"unknown\"。请仅返回JSON：{\"gesture\": \"open_palm or fist or unknown\", \"confidence\": 0.0到1.0之间的数字}",
        alloc), alloc);
    contentArr.PushBack(textPart, alloc);

    rapidjson::Value imagePart(rapidjson::kObjectType);
    imagePart.AddMember("type", "image_url", alloc);
    rapidjson::Value imageUrl(rapidjson::kObjectType);
    std::string url = "data:image/jpeg;base64," + imageB64;
    imageUrl.AddMember("url", rapidjson::Value(url.c_str(), alloc), alloc);
    imagePart.AddMember("image_url", imageUrl, alloc);
    contentArr.PushBack(imagePart, alloc);

    msg.AddMember("content", contentArr, alloc);
    messages.PushBack(msg, alloc);
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
    if (!isEnabled() || _requestInFlight || jpegData.empty()) return;

    _requestInFlight = true;
    std::string imageB64 = encodeBase64(jpegData);
    std::string body = buildRequestBody(imageB64, _endpointId);
    std::string apiKey = _apiKey;

    std::thread([this, body = std::move(body), apiKey = std::move(apiKey)]() {
        std::string response;
        CURL* curl = curl_easy_init();

        if (curl) {
            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            headers = curl_slist_append(headers, ("Authorization: Bearer " + apiKey).c_str());

            curl_easy_setopt(curl, CURLOPT_URL, _apiUrl.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

            CURLcode res = curl_easy_perform(curl);
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);

            if (res == CURLE_OK && !response.empty()) {
                Director::getInstance()->getScheduler()->performFunctionInCocosThread(
                    [this, response = std::move(response)]() {
                        rapidjson::Document doc;
                        doc.Parse(response.c_str());
                        if (!doc.HasParseError()) {
                            const auto& choices = doc["choices"];
                            if (choices.IsArray() && choices.Size() > 0) {
                                std::string content = choices[0]["message"]["content"].GetString();

                                rapidjson::Document inner;
                                inner.Parse(content.c_str());
                                if (!inner.HasParseError()) {
                                    std::string gesture = inner["gesture"].GetString();
                                    float confidence = inner["confidence"].GetFloat();
                                    if (_callback) _callback(gesture, confidence);
                                }
                            }
                        }
                        _requestInFlight = false;
                    }
                );
                return;
            }
        }

        _requestInFlight = false;
    }).detach();
}
