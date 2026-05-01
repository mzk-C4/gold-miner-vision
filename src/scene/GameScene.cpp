#include "GameScene.h"
#include "data/UserDataManager.h"
#include <QKeyEvent>
#include <QGraphicsSceneMouseEvent>
#include <QFont>
#include <QPen>
#include <QBrush>
#include <algorithm>

// ==================== 工厂方法 ====================
GameScene* GameScene::create(bool isBuyBomb, bool isBuyPotion,
                              bool isBuyDiamonds, bool isStoneBook,
                              int payMoney)
{
    auto *scene = new GameScene();
    scene->initLevel(isBuyBomb, isBuyPotion, isBuyDiamonds, isStoneBook, payMoney);
    return scene;
}

// ==================== 构造函数 ====================
GameScene::GameScene(QObject *parent)
    : QGraphicsScene(parent)
{
    setSceneRect(0, 0, 800, 600);

    // 设置背景色（天空 + 地面）
    setBackgroundBrush(QBrush(QColor(135, 206, 235))); // 天蓝色

    // 倒计时定时器（每秒触发）
    m_countdown = new QTimer(this);
    m_countdown->setInterval(1000);
    connect(m_countdown, &QTimer::timeout, this, &GameScene::onCountdownTick);

    // 游戏主循环定时器（用于碰撞检测等）
    m_gameTimer = new QTimer(this);
    m_gameTimer->setInterval(16); // ~60fps
}

GameScene::~GameScene()
{
    if (m_hook) {
        m_hook->removeFromScene();
        delete m_hook;
    }
}

// ==================== 关卡初始化 ====================
void GameScene::initLevel(bool isBuyBomb, bool isBuyPotion,
                           bool isBuyDiamonds, bool isStoneBook,
                           int payMoney)
{
    clear();

    m_hasBomb      = isBuyBomb;
    m_hasPotion    = isBuyPotion;
    m_hasDiamonds  = isBuyDiamonds;
    m_hasStoneBook = isStoneBook;
    m_payMoney     = payMoney;
    m_currentScore = 0;

    int stageNum = UserDataManager::getInstance()->getStageNum();

    // 加载关卡配置
    LevelData levelData = LevelConfig::loadLevel(stageNum);
    m_targetMoney   = levelData.targetMoney;
    m_timeRemaining = levelData.timeLimit;

    // 创建矿物
    for (const auto &mc : levelData.minerals) {
        Mineral *mineral = createMineralFromConfig(
            mc.type, mc.x, mc.y, mc.value, mc.weight, mc.size);

        // 应用道具效果
        if (m_hasDiamonds && mc.type == "diamond") {
            mineral->setValue(mineral->value() * 3);
        }
        if (m_hasStoneBook) {
            mineral->setValue(mineral->value() * 3);
        }

        addItem(mineral);
    }

    // 如果配置为空，插入默认矿物
    if (levelData.minerals.isEmpty()) {
        addItem(new SmallGold(300, 400, 200, 5));
        addItem(new BigGold(450, 380, 500, 10));
        addItem(new Diamond(150, 200, 600, 1));
        addItem(new Stone(550, 350, 20, 15));
    }

    // 添加地面线
    auto *ground = new QGraphicsRectItem(0, 580, 800, 20);
    ground->setPen(Qt::NoPen);
    ground->setBrush(QBrush(QColor(101, 67, 33))); // 泥土色
    addItem(ground);

    setupHook();
    setupUI(m_hasBomb);

    // 开始游戏
    m_countdown->start();
    m_gameTimer->start();

    // 设置焦点以接收键盘事件
    setFocus();
}

// ==================== 钩子初始化 ====================
void GameScene::setupHook()
{
    m_hook = new Hook(this, this);
    // 锚点位于顶部中央偏左（参考原项目 kWinSizeWidth * 0.48, kWinSizeHeight * 0.856）
    m_hook->init(QPointF(380, 80));

    // 应用力量药水效果
    if (m_hasPotion) {
        m_hook->setBackSpeed(12); // 回收速度增加20%
    }

    // 连接钩子信号
    connect(m_hook, &Hook::stateChanged, this, &GameScene::onHookStateChanged);
    connect(m_hook, &Hook::grabbed, this, &GameScene::onHookGrabbed);
    connect(m_hook, &Hook::retractComplete, this, &GameScene::onHookRetractComplete);
    connect(m_hook, &Hook::settlementDone, m_hook, &Hook::resetToSwinging);

    m_hook->startSwinging();
}

// ==================== UI 初始化 ====================
void GameScene::setupUI(bool showBomb)
{
    QFont font("Arial", 16, QFont::Bold);

    // 分数显示（左上角）
    m_scoreText = addText("$0", font);
    m_scoreText->setDefaultTextColor(QColor(255, 215, 0)); // 金色
    m_scoreText->setPos(20, 10);
    m_scoreText->setZValue(100);

    // 目标金额 + 关卡
    int stage = UserDataManager::getInstance()->getStageNum();
    auto *targetText = addText(
        QString("第%1关  目标: $%2").arg(stage).arg(m_targetMoney), font);
    targetText->setDefaultTextColor(Qt::white);
    targetText->setPos(150, 10);
    targetText->setZValue(100);

    // 倒计时（右上角）
    m_timerText = addText(QString("时间: %1s").arg(m_timeRemaining), font);
    m_timerText->setDefaultTextColor(Qt::white);
    m_timerText->setPos(650, 10);
    m_timerText->setZValue(100);

    // 炸药按钮
    if (showBomb) {
        auto *btn = new QPushButton("炸药");
        btn->setFixedSize(80, 40);
        btn->setStyleSheet(
            "QPushButton { background: #ff4444; color: white; "
            "font-size: 16px; border-radius: 5px; }"
            "QPushButton:hover { background: #ff6666; }");
        connect(btn, &QPushButton::clicked, this, &GameScene::onBombClicked);

        m_bombProxy = addWidget(btn);
        m_bombProxy->setPos(700, 520);
        m_bombProxy->setZValue(100);
        m_bombButton = btn;
    }

    // 操作提示
    auto *hintText = addText("[空格]放钩  [B]炸药  [Esc]返回", QFont("Arial", 10));
    hintText->setDefaultTextColor(QColor(200, 200, 200));
    hintText->setPos(10, 570);
    hintText->setZValue(100);
}

// ==================== 键盘输入（兜底方案） ====================
void GameScene::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Space:
        // 空格放钩
        if (m_hook && m_hook->state() == HookState::SWINGING) {
            m_hook->releaseHook();
        }
        break;
    case Qt::Key_B:
        // B 键炸药
        onBombClicked();
        break;
    case Qt::Key_Escape:
        emit backToHome();
        break;
    case Qt::Key_Left:
        // 手动控制钩子左摆
        if (m_hook && m_hook->state() == HookState::SWINGING) {
            m_hook->setSwingAngle(qMin(m_hook->swingAngle() + 5.0, 65.0));
        }
        break;
    case Qt::Key_Right:
        // 手动控制钩子右摆
        if (m_hook && m_hook->state() == HookState::SWINGING) {
            m_hook->setSwingAngle(qMax(m_hook->swingAngle() - 5.0, -65.0));
        }
        break;
    default:
        break;
    }
    QGraphicsScene::keyPressEvent(event);
}

void GameScene::keyReleaseEvent(QKeyEvent *event)
{
    QGraphicsScene::keyReleaseEvent(event);
}

// ==================== 鼠标点击（替代触摸） ====================
void GameScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_hook && m_hook->state() == HookState::SWINGING) {
        m_hook->releaseHook();
    }
    QGraphicsScene::mousePressEvent(event);
}

// ==================== 手势输入槽 ====================
void GameScene::onHandTilt(qreal angle)
{
    // 低通滤波消除抖动
    m_filteredAngle = kFilterAlpha * angle + (1.0 - kFilterAlpha) * m_filteredAngle;

    if (m_hook && m_hook->state() == HookState::SWINGING) {
        m_hook->setSwingAngle(qBound(-65.0, m_filteredAngle, 65.0));
    }
}

void GameScene::onHandOpen()
{
    if (m_hook && m_hook->state() == HookState::SWINGING) {
        m_hook->releaseHook();
    }
}

void GameScene::onHandFist()
{
    // 握拳加速回收（仅在收回状态有效）
    if (m_hook && m_hook->state() == HookState::RETRACTING) {
        m_hook->setBackSpeed(20);
    }
}

void GameScene::onHandGesture(const QString &gesture)
{
    if (gesture == "thumbs_up") {
        onBombClicked();
    }
}

// ==================== 倒计时 ====================
void GameScene::onCountdownTick()
{
    m_timeRemaining--;
    if (m_timerText) {
        m_timerText->setPlainText(QString("时间: %1s").arg(m_timeRemaining));
    }
    // 最后10秒变红
    if (m_timeRemaining <= 10 && m_timerText) {
        m_timerText->setDefaultTextColor(Qt::red);
    }

    if (m_timeRemaining <= 0) {
        m_countdown->stop();
        m_gameTimer->stop();
        checkLevelEnd();
    }
}

// ==================== 钩子状态回调 ====================
void GameScene::onHookStateChanged(HookState state)
{
    Q_UNUSED(state);
    // 可根据状态做 UI 变化
}

void GameScene::onHookGrabbed(Mineral *mineral)
{
    if (!mineral) return;

    // 矿物会随钩子回收，视觉上跟随 middleCircle
    // 在 Hook::onRetractTick 中更新位置
}

void GameScene::onHookRetractComplete()
{
    // 回收完成：计分并结算
    Mineral *grabbed = m_hook->grabbedMineral();
    if (grabbed) {
        int score = grabbed->value();
        m_currentScore += score;
        updateScoreDisplay();
        emit scoreChanged(m_currentScore);

        // 移除矿物
        removeItem(grabbed);
        delete grabbed;
        m_hook->setGrabbedMineral(nullptr);
    }

    m_hook->closeHook();
    m_hook->settle(); // 进入 SETTLING → 回到 SWINGING

    // 检查是否过关
    if (m_currentScore >= m_targetMoney) {
        m_countdown->stop();
        checkLevelEnd();
    }
}

// ==================== 炸药按钮 ====================
void GameScene::onBombClicked()
{
    if (!m_hasBomb) return;
    if (!m_hook || !m_hook->isOpenHook()) return;

    m_hasBomb = false;
    if (m_bombProxy) {
        m_bombProxy->setVisible(false);
    }
    m_hook->applyBomb();
}

// ==================== 辅助方法 ====================
void GameScene::updateScoreDisplay()
{
    if (m_scoreText) {
        m_scoreText->setPlainText(QString("$%1").arg(m_currentScore));
    }
}

void GameScene::checkLevelEnd()
{
    bool passed = m_currentScore >= m_targetMoney;
    showResult(passed);

    if (passed) {
        // 结算金币：当前得分 - 商店消费 + 原有金币
        int totalMoney = UserDataManager::getInstance()->getAllMoney()
                         + m_currentScore - m_payMoney;
        UserDataManager::getInstance()->setAllMoney(qMax(0, totalMoney));
        UserDataManager::getInstance()->advanceToNextStage();
        UserDataManager::getInstance()->save();

        emit levelPassed(m_currentScore);
    } else {
        emit levelFailed(m_currentScore);
    }
}

void GameScene::showResult(bool passed)
{
    QFont font("Arial", 28, QFont::Bold);
    m_resultText = addText(passed ? "过关!" : "失败!", font);
    m_resultText->setDefaultTextColor(passed ? QColor(0, 200, 0) : QColor(255, 0, 0));
    m_resultText->setPos(320, 250);
    m_resultText->setZValue(200);

    // 显示结果2秒后可操作
    auto *subText = addText(
        passed ? QString("获得 $%1!  点击继续").arg(m_currentScore)
               : QString("还差 $%1  点击重试").arg(m_targetMoney - m_currentScore),
        QFont("Arial", 14));
    subText->setDefaultTextColor(Qt::white);
    subText->setPos(280, 300);
    subText->setZValue(200);
}
