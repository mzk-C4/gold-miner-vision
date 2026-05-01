/**
 * HomeScene — 首页场景
 *
 * Logo 动画 + 开始按钮 + 音效/音乐开关
 * 参考原项目 MainRoot / 首页场景设计
 */
#ifndef HOMESCENE_H
#define HOMESCENE_H

#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsProxyWidget>
#include <QPushButton>
#include <QPropertyAnimation>

class HomeScene : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit HomeScene(QObject *parent = nullptr);

signals:
    void startGame();

private:
    void setupLogo();
    void setupButtons();
    void playLogoAnimation();

    QGraphicsTextItem    *m_titleText  = nullptr;
    QGraphicsProxyWidget *m_startProxy = nullptr;
    QGraphicsProxyWidget *m_musicProxy = nullptr;
    QGraphicsProxyWidget *m_soundProxy = nullptr;
    QPropertyAnimation   *m_logoAnim  = nullptr;
};

#endif // HOMESCENE_H
