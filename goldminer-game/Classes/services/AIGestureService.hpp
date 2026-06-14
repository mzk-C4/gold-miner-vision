#pragma once

#include "cocos2d.h"
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <vector>

class AIGestureService {
public:
    using ResultCallback = std::function<void(const std::string& gesture, float confidence)>;

    static AIGestureService* getInstance();

    /// Set the API endpoint ID (ep-xxxxxxxxxx)
    void setEndpointId(const std::string& id) { _endpointId = id; }
    /// Set API key for authentication
    void setApiKey(const std::string& key) { _apiKey = key; }

    /// Send JPEG frame for gesture analysis (non-blocking, callback on cocos thread)
    void sendFrame(const std::vector<unsigned char>& jpegData);
    void setCallback(ResultCallback cb) { _callback = std::move(cb); }

    bool isEnabled() const { return !_endpointId.empty() && !_apiKey.empty(); }

private:
    AIGestureService() = default;
    ~AIGestureService();

    static std::string encodeBase64(const std::vector<unsigned char>& data);
    static std::string buildRequestBody(const std::string& imageB64, const std::string& model);
    static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata);

    std::string _endpointId;
    std::string _apiKey;
    std::string _apiUrl = "https://ark.cn-beijing.volces.com/api/v3/chat/completions";
    ResultCallback _callback;

    std::atomic<bool> _requestInFlight{false};
};
