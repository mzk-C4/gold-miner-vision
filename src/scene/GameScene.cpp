#include "GameScene.h"
#include "data/UserDataManager.h"
#include <QKeyEvent>
#include <QGraphicsSceneMouseEvent>
#include <QFont>
#include <QPen>
#include <QBrush>

GameScene* GameScene::create(bool bomb, bool potion, bool diamonds,
                              bool stoneBook, int pay)
{
    auto *s = new GameScene();
    s->initLevel(bomb, potion, diamonds, stoneBook, pay);
    return s;
}

GameScene::GameScene(QObject *parent)
    : QGraphicsScene(parent)
{
    setSceneRect(0, 0, 800, 600);
    setBackgroundBrush(QBrush(QColor(135, 206, 235)));

    m_countdown = new QTimer(this);
    m_countdown->setInterval(1000);
    connect(m_countdown, &QTimer::timeout, this, &GameScene::onCountdownTick);
}

GameScene::~GameScene()
{
    cleanupGame();
}

void GameScene::initLevel(bool bomb, bool potion, bool diamonds,
                           bool stoneBook, int pay)
{
    clear();

    m_hasBomb      = bomb;
    m_hasPotion    = potion;
    m_hasDiamonds  = diamonds;
    m_hasStoneBook = stoneBook;
    m_payMoney     = pay;
    m_currentScore = 0;
    m_filteredAngle = 0.0;

    int stage = UserDataManager::getInstance()->getStageNum();
    LevelData levelData = LevelConfig::loadLevel(stage);
    m_targetMoney   = levelData.targetMoney;
    m_timeRemaining = levelData.timeLimit;

    loadMinerals(stage);
    setupHook();
    setupUI(m_hasBomb);
    setFocus();
    m_countdown->start();
}

void GameScene::loadMinerals(int stageNum)
{
    LevelData data = LevelConfig::loadLevel(stageNum);

    for (const auto &mc : data.minerals) {
        Mineral *m = createMineralFromConfig(mc.type, mc.x, mc.y,
                                              mc.value, mc.weight, mc.size);
        if (m_hasDiamonds && mc.type == "diamond")
            m->setValue(m->value() * 3);
        if (m_hasStoneBook)
            m->setValue(m->value() * 3);
        addItem(m);
    }

    // 无配置时使用默认矿物
    if (data.minerals.isEmpty()) {
        addItem(new SmallGold(300, 350, 200, 5));
        addItem(new SmallGold(500, 400, 200, 5));
        addItem(new BigGold(400, 350, 500, 10));
        addItem(new Diamond(200, 280, 600, 1));
        addItem(new Stone(550, 420, 20, 15));
        addItem(new Stone(150, 430, 20, 15));
    }

    // 地面线
    auto *ground = new QGraphicsRectItem(0, 580, 800, 20);
    ground->setPen(Qt::NoPen);
    ground->setBrush(QBrush(QColor(101, 67, 33)));
    addItem(ground);
}

void GameScene::setupHook()
{
    m_hook = new Hook(this, this);
    m_hook->init(QPointF(380, 80));
    if (m_hasPotion) m_hook->setBackSpeed(12);

    connect(m_hook, &Hook::stateChanged, this, &GameScene::onHookStateChanged);
    connect(m_hook, &Hook::grabbed, this, &GameScene::onHookGrabbed);
    connect(m_hook, &Hook::retractComplete, this, &GameScene::onHookRetractComplete);
    connect(m_hook, &Hook::settlementDone, m_hook, &Hook::resetToSwinging);

    m_hook->startSwinging();
}

void GameScene::setupUI(bool showBomb)
{
    QFont font("SimHei", 16, QFont::Bold);
    int stage = UserDataManager::getInstance()->getStageNum();

    // 分数
    m_scoreText = addText("$0", font);
    m_scoreText->setDefaultTextColor(QColor(255, 215, 0));
    m_scoreText->setPos(20, 10);
    m_scoreText->setZValue(100);

    // 关卡与目标
    m_targetText = addText(
        QString("第%1关  目标: $%2").arg(stage).arg(m_targetMoney), font);
    m_targetText->setDefaultTextColor(Qt::white);
    m_targetText->setPos(150, 10);
    m_targetText->setZValue(100);

    // 倒计时
    m_timerText = addText(QString("时间: %1s").arg(m_timeRemaining), font);
    m_timerText->setDefaultTextColor(Qt::white);
    m_timerText->setPos(650, 10);
    m_timerText->setZValue(100);

    // 炸药按钮
    if (showBomb) {
        auto *btn = new QPushButton("炸药");
        btn->setFixedSize(80, 40);
        btn->setStyleSheet(
            "QPushButton{background:#ff4444;color:white;font-size:16px;"
            "border-radius:5px;}QPushButton:hover{background:#ff6666;}");
        connect(btn, &QPushButton::clicked, this, &GameScene::onBombClicked);
        m_bombProxy = addWidget(btn);
        m_bombProxy->setPos(700, 520);
        m_bombProxy->setZValue(100);
    }

    // 操作提示
    auto *hint = addText("[空格]放钩  [←→]控制  [B]炸药  [Esc]返回",
                          QFont("SimHei", 10));
    hint->setDefaultTextColor(QColor(200, 200, 200));
    hint->setPos(10, 570);
    hint->setZValue(100);
}

// ==================== 键盘输入（兜底方案） ====================

void GameScene::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Space:
        if (m_hook && m_hook->state() == HookState::SWINGING)
            m_hook->releaseHook();
        break;
    case Qt::Key_B:
        onBombClicked();
        break;
    case Qt::Key_Escape:
        emit backToHome();
        break;
    case Qt::Key_Left:
        // 左键 → 钩子向左摆（负角度）
        if (m_hook && m_hook->state() == HookState::SWINGING)
            m_hook->setSwingAngle(qMax(m_hook->swingAngle() - 5.0, -65.0));
        break;
    case Qt::Key_Right:
        // 右键 → 钩子向右摆（正角度）
        if (m_hook && m_hook->state() == HookState::SWINGING)
            m_hook->setSwingAngle(qMin(m_hook->swingAngle() + 5.0, 65.0));
        break;
    default:
        break;
    }
    QGraphicsScene::keyPressEvent(event);
}

void GameScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_hook && m_hook->state() == HookState::SWINGING)
        m_hook->releaseHook();
    QGraphicsScene::mousePressEvent(event);
}

// ==================== 手势输入 ====================

void GameScene::onHandTilt(qreal angle)
{
    m_filteredAngle = 0.3 * angle + 0.7 * m_filteredAngle;
    if (m_hook && m_hook->state() == HookState::SWINGING)
        m_hook->setSwingAngle(qBound(-65.0, m_filteredAngle, 65.0));
}

void GameScene::onHandOpen()
{
    if (m_hook && m_hook->state() == HookState::SWINGING)
        m_hook->releaseHook();
}

void GameScene::onHandFist()
{
    if (m_hook && m_hook->state() == HookState::RETRACTING)
        m_hook->setBackSpeed(20);
}

void GameScene::onHandGesture(const QString &gesture)
{
    if (gesture == "thumbs_up")
        onBombClicked();
}

// ==================== 倒计时 ====================

void GameScene::onCountdownTick()
{
    m_timeRemaining--;
    if (m_timerText)
        m_timerText->setPlainText(QString("时间: %1s").arg(m_timeRemaining));
    if (m_timeRemaining <= 10 && m_timerText)
        m_timerText->setDefaultTextColor(Qt::red);
    if (m_timeRemaining <= 0) {
        m_countdown->stop();
        checkLevelEnd();
    }
}

// ==================== 钩子回调 ====================

void GameScene::onHookStateChanged(HookState state) { Q_UNUSED(state); }

void GameScene::onHookGrabbed(Mineral *mineral) { Q_UNUSED(mineral); }

void GameScene::onHookRetractComplete()
{
    Mineral *grabbed = m_hook->grabbedMineral();
    if (grabbed) {
        m_currentScore += grabbed->value();
        updateScoreDisplay();
        removeItem(grabbed);
        delete grabbed;
        m_hook->setGrabbedMineral(nullptr);
    }
    m_hook->closeHook();
    m_hook->settle();

    if (m_currentScore >= m_targetMoney) {
        m_countdown->stop();
        checkLevelEnd();
    }
}

// ==================== 炸药 ====================

void GameScene::onBombClicked()
{
    if (!m_hasBomb || !m_hook || !m_hook->isOpenHook()) return;
    m_hasBomb = false;
    if (m_bombProxy) m_bombProxy->setVisible(false);
    m_hook->applyBomb();
}

// ==================== 辅助 ====================

void GameScene::updateScoreDisplay()
{
    if (m_scoreText)
        m_scoreText->setPlainText(QString("$%1").arg(m_currentScore));
}

void GameScene::checkLevelEnd()
{
    bool passed = m_currentScore >= m_targetMoney;
    showResult(passed);

    auto *udm = UserDataManager::getInstance();
    if (passed) {
        int total = udm->getAllMoney() + m_currentScore - m_payMoney;
        udm->setAllMoney(qMax(0, total));
        udm->advanceToNextStage();
        emit levelPassed(m_currentScore);
    } else {
        emit levelFailed(m_currentScore);
    }
}

void GameScene::showResult(bool passed)
{
    QFont font("SimHei", 28, QFont::Bold);
    auto *text = addText(passed ? "过关!" : "失败! 重试中...", font);
    text->setDefaultTextColor(passed ? QColor(0, 200, 0) : QColor(255, 0, 0));
    text->setPos(320, 250);
    text->setZValue(200);
}

void GameScene::cleanupGame()
{
    m_countdown->stop();
    if (m_hook) {
        m_hook->removeFromScene();
        delete m_hook;
        m_hook = nullptr;
    }
}
