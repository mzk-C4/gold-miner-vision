/**
 * GameScene — 核心游戏场景（继承 QGraphicsScene）
 *
 * 管理：矿物棋盘、钩子系统、60秒倒计时、分数结算
 * 双模输入：键盘（兜底）+ 手势信号（从 HandTracker 接收）
 * 参考原项目 Game::init / createScene / physicsBegin 等设计
 */
#ifndef GAMESCENE_H
#define GAMESCENE_H

#include <QGraphicsScene>
#include <QTimer>
#include <QLabel>
#include <QGraphicsProxyWidget>
#include <QPushButton>
#include "core/Hook.h"
#include "core/Mineral.h"
#include "data/LevelConfig.h"

class GameScene : public QGraphicsScene
{
    Q_OBJECT

public:
    // 工厂方法：传入道具购买状态和消费金额
    static GameScene* create(bool isBuyBomb, bool isBuyPotion,
                              bool isBuyDiamonds, bool isStoneBook,
                              int payMoney);

    explicit GameScene(QObject *parent = nullptr);
    ~GameScene() override;

    // 初始化关卡（传入道具状态）
    void initLevel(bool isBuyBomb, bool isBuyPotion,
                   bool isBuyDiamonds, bool isStoneBook, int payMoney);

signals:
    void levelPassed(int score);       // 过关
    void levelFailed(int score);       // 失败
    void backToHome();                 // 返回首页
    void scoreChanged(int totalScore);

public slots:
    // ========== 手势输入槽（从 HandTracker 接收） ==========
    void onHandTilt(qreal angle);          // 手部倾斜 → 钩子摆动角度
    void onHandOpen();                     // 手掌张开 → 放钩
    void onHandFist();                     // 握拳 → 加速回收
    void onHandGesture(const QString &gesture); // 其他手势

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

private slots:
    void onCountdownTick();                // 倒计时每秒回调
    void onHookStateChanged(HookState state);
    void onHookGrabbed(Mineral *mineral);
    void onHookRetractComplete();
    void onBombClicked();

private:
    void loadMinerals(int stageNum);       // 加载关卡矿物
    void setupUI(bool showBomb);           // 创建分数/计时器等 UI
    void setupHook();
    void updateScoreDisplay();
    void checkLevelEnd();                  // 检查是否过关
    void showResult(bool passed);          // 显示过关/失败结果

    // ========== 道具状态 ==========
    bool m_hasBomb      = false;
    bool m_hasPotion    = false;
    bool m_hasDiamonds  = false;
    bool m_hasStoneBook = false;
    int  m_payMoney     = 0;       // 在商店花费的金币数

    // ========== 游戏数据 ==========
    int m_targetMoney   = 650;     // 本关目标金额
    int m_currentScore  = 0;       // 本关当前得分
    int m_timeRemaining = 60;      // 剩余秒数

    // ========== 核心组件 ==========
    Hook   *m_hook       = nullptr;
    QTimer *m_countdown  = nullptr;  // 1 秒倒计时
    QTimer *m_gameTimer  = nullptr;  // 主循环（碰撞检测等）

    // ========== UI 元素 ==========
    QGraphicsTextItem    *m_scoreText   = nullptr;
    QGraphicsTextItem    *m_timerText   = nullptr;
    QGraphicsProxyWidget *m_bombProxy   = nullptr;
    QPushButton          *m_bombButton  = nullptr;
    QGraphicsTextItem    *m_resultText  = nullptr; // 过关/失败提示

    // ========== 手势滤波 ==========
    qreal m_filteredAngle = 0.0;
    static constexpr qreal kFilterAlpha = 0.3; // 低通滤波系数
};

#endif // GAMESCENE_H
