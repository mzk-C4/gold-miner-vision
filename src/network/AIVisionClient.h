/**
 * AIVisionClient — 豆包/火山引擎多模态视觉 API 客户端
 *
 * 将摄像头帧编码为 Base64，通过 HTTP POST 发送到视觉 API
 * 接收手势分析结果（JSON 格式）
 * 超时或失败时自动降级到本地 CV 模式
 *
 * 模型：Doubao-1.5-vision-pro
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

    // ========== API 配置 ==========
    void setApiKey(const QString &key)    { m_apiKey = key; }
    void setApiUrl(const QString &url)    { m_apiUrl = url; }
    void setTimeout(int ms)               { m_timeoutMs = ms; }

    // ========== 请求分析 ==========
    // 发送 Base64 编码的图片帧到视觉 API
    void analyzeFrame(const QByteArray &imageBase64);
    void analyzeFrameRaw(const QByteArray &imageData); // 发送原始图片数据

signals:
    // ========== 分析结果信号 ==========
    void handTilt(qreal angle);            // 手部倾斜角度
    void handOpen();                       // 手掌张开
    void handFist();                       // 握拳
    void handGesture(const QString &name); // 特定手势名称

    // ========== 状态信号 ==========
    void apiError(const QString &error);
    void apiTimeout();                     // 超时 → 触发降级
    void modeDegraded();                   // 已降级到本地模式

private slots:
    void onReplyFinished(QNetworkReply *reply);
    void onTimeout();

private:
    void parseResponse(const QJsonObject &json);
    QString buildPrompt();                 // 构建发给 API 的提示词

    QNetworkAccessManager *m_networkMgr = nullptr;
    QTimer                *m_timeoutTimer = nullptr;
    QNetworkReply         *m_pendingReply = nullptr;

    QString m_apiKey = "09319510-56ea-4f89-9cdf-ced640510471";
    QString m_apiUrl = "https://ark.cn-beijing.volces.com/api/v3/chat/completions";
    int     m_timeoutMs = 3000;  // 3 秒超时
    bool    m_isRequesting = false;

    // 连续失败计数（用于自动降级）
    int     m_consecutiveFailures = 0;
    static constexpr int kMaxFailures = 3;
};

#endif // AIVISIONCLIENT_H
