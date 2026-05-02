/**
 * AIVisionClient — 豆包多模态视觉 API 客户端
 * 将摄像头帧编码为 Base64 发送到视觉 API 进行手势分析
 * 超时或连续失败时自动降级到本地 CV 模式
 */
#ifndef AIVISIONCLIENT_H
#define AIVISIONCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

class AIVisionClient : public QObject
{
    Q_OBJECT

public:
    explicit AIVisionClient(QObject *parent = nullptr);

    void setApiKey(const QString &key) { m_apiKey = key; }
    void setApiUrl(const QString &url) { m_apiUrl = url; }
    void setTimeout(int ms)            { m_timeoutMs = ms; }

    void analyzeFrame(const QByteArray &imageBase64);

signals:
    void handTilt(qreal angle);
    void handOpen();
    void handFist();
    void handGesture(const QString &name);

    void apiError(const QString &error);
    void apiTimeout();
    void modeDegraded();

private slots:
    void onReplyFinished(QNetworkReply *reply);
    void onTimeout();

private:
    void parseResponse(const QJsonObject &json);
    QString buildPrompt();

    QNetworkAccessManager *m_networkMgr = nullptr;
    QTimer    *m_timeoutTimer = nullptr;
    QNetworkReply *m_pendingReply = nullptr;

    QString m_apiKey  = "09319510-56ea-4f89-9cdf-ced640510471";
    QString m_apiUrl  = "https://ark.cn-beijing.volces.com/api/v3/chat/completions";
    int     m_timeoutMs = 3000;
    bool    m_isRequesting = false;

    int m_consecutiveFailures = 0;
    static constexpr int kMaxFailures = 3;
};

#endif // AIVISIONCLIENT_H
