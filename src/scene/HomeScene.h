/**
 * HomeScene — 首页场景
 * Logo 缩放弹出动画 + 开始按钮 + 音效/音乐开关
 */
#ifndef HOMESCENE_H
#define HOMESCENE_H

#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsProxyWidget>
#include <QVariantAnimation>

class HomeScene : public QGraphicsScene
{
    Q_OBJECT
    Q_PROPERTY(qreal logoScale READ logoScale WRITE setLogoScale)

public:
    explicit HomeScene(QObject *parent = nullptr);

    qreal logoScale() const { return m_logoScale; }
    void setLogoScale(qreal s);

signals:
    void startGame();

private:
    void setupUI();
    void playLogoAnimation();

    QGraphicsTextItem *m_titleText = nullptr;
    qreal m_logoScale = 0.1;
    QVariantAnimation *m_logoAnim = nullptr;
};

#endif // HOMESCENE_H
