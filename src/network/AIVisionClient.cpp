#include "AIVisionClient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>

AIVisionClient::AIVisionClient(QObject *parent)
    : QObject(parent)
{
    m_networkMgr = new QNetworkAccessManager(this);
    connect(m_networkMgr, &QNetworkAccessManager::finished,
            this, &AIVisionClient::onReplyFinished);

    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &AIVisionClient::onTimeout);
}

// ==================== 发送分析请求 ====================

void AIVisionClient::analyzeFrame(const QByteArray &imageBase64)
{
    if (m_isRequesting) return;  // 上一帧还在处理中
    if (m_apiKey.isEmpty()) {
        emit apiError("API Key 未设置");
        return;
    }

    m_isRequesting = true;

    // 构建请求体
    QJsonObject requestBody;
    requestBody["model"] = "doubao-seedance-1-5-pro-251215";

    QJsonArray messages;
    QJsonObject userMessage;
    userMessage["role"] = "user";

    // 多模态内容：文本 + 图片
    QJsonArray content;
    QJsonObject textPart;
    textPart["type"] = "text";
    textPart["text"] = buildPrompt();
    content.append(textPart);

    QJsonObject imagePart;
    imagePart["type"]   = "image_url";
    imagePart["image_url"] = QJsonObject{{"url", QString("data:image/jpeg;base64,") + imageBase64}};
    content.append(imagePart);

    userMessage["content"] = content;
    messages.append(userMessage);
    requestBody["messages"] = messages;
    requestBody["max_tokens"] = 100;

    // HTTP POST
    QUrl url(m_apiUrl);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());

    QJsonDocument doc(requestBody);
    m_pendingReply = m_networkMgr->post(request, doc.toJson());

    // 启动超时定时器
    m_timeoutTimer->start(m_timeoutMs);
}

void AIVisionClient::analyzeFrameRaw(const QByteArray &imageData)
{
    // 将原始图片数据编码为 Base64
    analyzeFrame(imageData.toBase64());
}

// ==================== 请求完成回调 ====================

void AIVisionClient::onReplyFinished(QNetworkReply *reply)
{
    m_isRequesting = false;
    m_timeoutTimer->stop();
    m_pendingReply = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        m_consecutiveFailures++;
        emit apiError(reply->errorString());

        if (m_consecutiveFailures >= kMaxFailures) {
            m_consecutiveFailures = 0;
            emit modeDegraded();
        }
        reply->deleteLater();
        return;
    }

    m_consecutiveFailures = 0;  // 成功后重置

    QByteArray responseData = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if (!doc.isObject()) {
        emit apiError("Invalid JSON response");
        reply->deleteLater();
        return;
    }

    // 解析响应
    QJsonObject root = doc.object();
    QJsonArray choices = root["choices"].toArray();
    if (choices.isEmpty()) {
        emit apiError("No choices in response");
        reply->deleteLater();
        return;
    }

    QString content = choices[0].toObject()["message"]
                          .toObject()["content"].toString();
    // 尝试解析 API 返回的 JSON 格式手势数据
    QJsonObject resultObj = QJsonDocument::fromJson(content.toUtf8()).object();
    if (!resultObj.isEmpty()) {
        parseResponse(resultObj);
    }

    reply->deleteLater();
}

void AIVisionClient::onTimeout()
{
    if (m_pendingReply) {
        m_pendingReply->abort();
        m_pendingReply->deleteLater();
        m_pendingReply = nullptr;
    }
    m_isRequesting = false;
    m_consecutiveFailures++;

    emit apiTimeout();

    if (m_consecutiveFailures >= kMaxFailures) {
        m_consecutiveFailures = 0;
        emit modeDegraded();
    }
}

// ==================== 响应解析 ====================

void AIVisionClient::parseResponse(const QJsonObject &json)
{
    // 解析 API 返回的手势数据：
    // {"gesture": "open_palm" | "fist" | "thumbs_up" | "none",
    //  "tilt_angle": -30.5}
    QString gesture = json["gesture"].toString();

    if (gesture == "open_palm") {
        emit handOpen();
    } else if (gesture == "fist") {
        emit handFist();
    } else if (gesture == "thumbs_up") {
        emit handGesture("thumbs_up");
    }

    if (json.contains("tilt_angle")) {
        qreal angle = json["tilt_angle"].toDouble();
        emit handTilt(angle);
    }
}

// ==================== 提示词构建 ====================

QString AIVisionClient::buildPrompt()
{
    return QString(
        "分析这张图片中的人手手势。图片中的人手戴着蓝色手套。"
        "请返回以下JSON格式，不要包含任何其他文字：\n"
        "{\n"
        "  \"gesture\": \"open_palm\" | \"fist\" | \"thumbs_up\" | \"none\",\n"
        "  \"tilt_angle\": <手部相对于竖直方向的角度，-65到65之间的数字>\n"
        "}\n\n"
        "判断规则：\n"
        "- 五指张开 → open_palm\n"
        "- 握拳 → fist\n"
        "- 竖起大拇指 → thumbs_up\n"
        "- 手部左右倾斜角度，以度为单位，向左为正，向右为负"
    );
}
