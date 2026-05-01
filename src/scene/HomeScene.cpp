#include "HomeScene.h"
#include "data/UserDataManager.h"
#include <QFont>
#include <QBrush>
#include <QGraphicsEffect>

HomeScene::HomeScene(QObject *parent)
    : QGraphicsScene(parent)
{
    setSceneRect(0, 0, 800, 600);

    // 背景
    setBackgroundBrush(QBrush(QColor(30, 30, 60))); // 深蓝色背景

    setupLogo();
    setupButtons();
    playLogoAnimation();
}

void HomeScene::setupLogo()
{
    // 游戏标题
    m_titleText = addText("黄金矿工", QFont("SimHei", 48, QFont::Bold));
    m_titleText->setDefaultTextColor(QColor(255, 215, 0)); // 金色
    m_titleText->setPos(250, 120);

    // 副标题
    auto *subtitle = addText("手势控制版 v1.0", QFont("Arial", 14));
    subtitle->setDefaultTextColor(QColor(200, 200, 200));
    subtitle->setPos(320, 190);

    // 显示用户信息
    auto *udm = UserDataManager::getInstance();
    auto *info = addText(
        QString("金币: $%1    当前关卡: %2")
            .arg(udm->getAllMoney()).arg(udm->getStageNum()),
        QFont("Arial", 12));
    info->setDefaultTextColor(QColor(180, 180, 180));
    info->setPos(280, 220);
}

void HomeScene::setupButtons()
{
    // 开始按钮
    auto *startBtn = new QPushButton("开始游戏");
    startBtn->setFixedSize(200, 60);
    startBtn->setStyleSheet(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #ffd700, stop:1 #ff8c00);"
        "  color: #333; font-size: 22px; font-weight: bold;"
        "  border-radius: 10px; border: 2px solid #b8860b;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #ffe44d, stop:1 #ffa500);"
        "}");
    connect(startBtn, &QPushButton::clicked, this, &HomeScene::startGame);

    m_startProxy = addWidget(startBtn);
    m_startProxy->setPos(300, 320);

    // 音乐开关
    auto *musicBtn = new QPushButton(
        UserDataManager::getInstance()->isMusicMuted() ? "音乐: 关" : "音乐: 开");
    musicBtn->setFixedSize(120, 36);
    musicBtn->setStyleSheet(
        "QPushButton { background: #555; color: white; border-radius: 5px; }"
        "QPushButton:hover { background: #777; }");
    connect(musicBtn, &QPushButton::clicked, this, [musicBtn]() {
        auto *udm = UserDataManager::getInstance();
        bool muted = !udm->isMusicMuted();
        udm->setMusicMuted(muted);
        musicBtn->setText(muted ? "音乐: 关" : "音乐: 开");
    });
    m_musicProxy = addWidget(musicBtn);
    m_musicProxy->setPos(250, 420);

    // 音效开关
    auto *soundBtn = new QPushButton(
        UserDataManager::getInstance()->isSoundMuted() ? "音效: 关" : "音效: 开");
    soundBtn->setFixedSize(120, 36);
    soundBtn->setStyleSheet(
        "QPushButton { background: #555; color: white; border-radius: 5px; }"
        "QPushButton:hover { background: #777; }");
    connect(soundBtn, &QPushButton::clicked, this, [soundBtn]() {
        auto *udm = UserDataManager::getInstance();
        bool muted = !udm->isSoundMuted();
        udm->setSoundMuted(muted);
        soundBtn->setText(muted ? "音效: 关" : "音效: 开");
    });
    m_soundProxy = addWidget(soundBtn);
    m_soundProxy->setPos(430, 420);

    // 操作说明
    auto *helpText = addText(
        "操作: [空格]放钩  [←→]控制方向  [B]炸药  [Esc]返回\n"
        "手势: 手部倾斜→钩子摆动  手掌张开→放钩",
        QFont("Arial", 10));
    helpText->setDefaultTextColor(QColor(150, 150, 150));
    helpText->setPos(200, 500);
}

void HomeScene::playLogoAnimation()
{
    // Logo 缩放弹出动画
    m_titleText->setScale(0.1);
    m_logoAnim = new QPropertyAnimation(m_titleText, "scale", this);
    m_logoAnim->setDuration(800);
    m_logoAnim->setStartValue(0.1);
    m_logoAnim->setEndValue(1.0);
    m_logoAnim->setEasingCurve(QEasingCurve::OutBack);
    m_logoAnim->start(QAbstractAnimation::DeleteWhenStopped);
}
