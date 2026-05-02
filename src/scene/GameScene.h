/**
 * GameScene — 核心游戏场景
 *
 * 管理：矿物棋盘、钩子系统、60秒倒计时、分数结算
 * 双模输入：键盘（兜底）+ 手势信号（从 HandTracker 接收）
 */
#ifndef GAMESCENE_H
#define GAMESCENE_H

#include <QGraphicsScene>
#include <QTimer>
#include <QGraphicsTextItem>
#include <QGraphicsProxyWidget>
#include <QPushButton>
#include "core/Hook.h"
#include "core/Mineral.h"
#include "data/LevelConfig.h"

class GameScene : public QGraphicsScene
{
    Q_OBJECT

public:
    static GameScene* create(bool isBuyBomb, bool isBuyPotion,
                              bool isBuyDiamonds, bool isStoneBook,
                              int payMoney);

    explicit GameScene(QObject *parent = nullptr);
    ~GameScene() override;

    void initLevel(bool isBuyBomb, bool isBuyPotion,
                   bool isBuyDiamonds, bool isStoneBook, int payMoney);

signals:
    void levelPassed(int score);
    void levelFailed(int score);
    void backToHome();

public slots:
    void onHandTilt(qreal angle);
    void onHandOpen();
    void onHandFist();
    void onHandGesture(const QString &gesture);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

private slots:
    void onCountdownTick();
    void onHookStateChanged(HookState state);
    void onHookGrabbed(Mineral *mineral);
    void onHookRetractComplete();
    void onBombClicked();

private:
    void loadMinerals(int stageNum);
    void setupUI(bool showBomb);
    void setupHook();
    void updateScoreDisplay();
    void checkLevelEnd();
    void showResult(bool passed);
    void cleanupGame();

    // 道具状态
    bool m_hasBomb      = false;
    bool m_hasPotion    = false;
    bool m_hasDiamonds  = false;
    bool m_hasStoneBook = false;
    int  m_payMoney     = 0;

    // 游戏数据
    int m_targetMoney   = 650;
    int m_currentScore  = 0;
    int m_timeRemaining = 60;

    // 核心组件
    Hook   *m_hook       = nullptr;
    QTimer *m_countdown  = nullptr;

    // UI
    QGraphicsTextItem    *m_scoreText  = nullptr;
    QGraphicsTextItem    *m_timerText  = nullptr;
    QGraphicsTextItem    *m_targetText = nullptr;
    QGraphicsProxyWidget *m_bombProxy  = nullptr;

    // 低通滤波
    qreal m_filteredAngle = 0.0;
};

#endif // GAMESCENE_H
