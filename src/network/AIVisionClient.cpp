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

void AIVisionClient::analyzeFrame(const QByteArray &imageBase64)
{
    if (m_isRequesting) return;
    if (m_apiKey.isEmpty()) return;

    m_isRequesting = true;

    QJsonObject body;
    body["model"] = "doubao-seedance-1-5-pro-251215";

    QJsonObject textPart;
    textPart["type"] = "text";
    textPart["text"] = buildPrompt();

    QJsonObject imagePart;
    imagePart["type"] = "image_url";
    imagePart["image_url"] = QJsonObject{
        {"url", QString("data:image/jpeg;base64,") + imageBase64}
    };

    QJsonArray content;
    content.append(textPart);
    content.append(imagePart);

    QJsonObject msg;
    msg["role"] = "user";
    msg["content"] = content;

    QJsonArray messages;
    messages.append(msg);
    body["messages"] = messages;
    body["max_tokens"] = 100;

    QUrl url(m_apiUrl);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());

    m_pendingReply = m_networkMgr->post(request, QJsonDocument(body).toJson());
    m_timeoutTimer->start(m_timeoutMs);
}

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

    m_consecutiveFailures = 0;

    QByteArray data = reply->readAll();
    QJsonObject root = QJsonDocument::fromJson(data).object();
    QJsonArray choices = root["choices"].toArray();

    if (choices.isEmpty()) {
        emit apiError("No choices in response");
        reply->deleteLater();
        return;
    }

    QString content = choices[0].toObject()["message"]
                          .toObject()["content"].toString();
    QJsonObject result = QJsonDocument::fromJson(content.toUtf8()).object();
    if (!result.isEmpty())
        parseResponse(result);

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

void AIVisionClient::parseResponse(const QJsonObject &json)
{
    QString gesture = json["gesture"].toString();
    if (gesture == "open_palm")       emit handOpen();
    else if (gesture == "fist")       emit handFist();
    else if (gesture == "thumbs_up")  emit handGesture("thumbs_up");

    if (json.contains("tilt_angle"))
        emit handTilt(json["tilt_angle"].toDouble());
}

QString AIVisionClient::buildPrompt()
{
    return QString(
        "分析图片中的人手手势（手戴蓝色手套）。返回JSON格式（不含其他文字）：\n"
        "{\n"
        "  \"gesture\": \"open_palm\" | \"fist\" | \"thumbs_up\" | \"none\",\n"
        "  \"tilt_angle\": <手部竖直方向角度，-65到65，左倾为正右倾为负>\n"
        "}\n"
        "判断规则：五指张开→open_palm，握拳→fist，竖拇指→thumbs_up");
}
