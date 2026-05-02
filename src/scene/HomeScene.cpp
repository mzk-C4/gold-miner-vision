#include "HomeScene.h"
#include "data/UserDataManager.h"
#include <QFont>
#include <QBrush>
#include <QPushButton>
#include <QGraphicsDropShadowEffect>

HomeScene::HomeScene(QObject *parent)
    : QGraphicsScene(parent)
{
    setSceneRect(0, 0, 800, 600);
    setBackgroundBrush(QBrush(QColor(30, 30, 60)));
    setupUI();
    playLogoAnimation();
}

void HomeScene::setLogoScale(qreal s)
{
    m_logoScale = s;
    if (m_titleText) {
        QTransform t;
        t.scale(s, s);
        m_titleText->setTransform(t);
    }
}

void HomeScene::setupUI()
{
    // 标题
    m_titleText = addText("黄金矿工", QFont("SimHei", 48, QFont::Bold));
    m_titleText->setDefaultTextColor(QColor(255, 215, 0));
    m_titleText->setPos(250, 120);
    m_titleText->setTransformOriginPoint(m_titleText->boundingRect().center());

    // 为标题添加发光效果
    auto *effect = new QGraphicsDropShadowEffect();
    effect->setBlurRadius(20);
    effect->setColor(QColor(255, 200, 0, 150));
    effect->setOffset(0, 0);
    m_titleText->setGraphicsEffect(effect);

    // 副标题
    auto *sub = addText("手势控制版 v1.0", QFont("Arial", 14));
    sub->setDefaultTextColor(QColor(200, 200, 200));
    sub->setPos(320, 190);

    // 用户信息
    auto *udm = UserDataManager::getInstance();
    auto *info = addText(
        QString("金币: $%1    当前关卡: %2")
            .arg(udm->getAllMoney()).arg(udm->getStageNum()),
        QFont("SimHei", 12));
    info->setDefaultTextColor(QColor(180, 180, 180));
    info->setPos(280, 220);

    // 开始按钮
    auto *btn = new QPushButton("开始游戏");
    btn->setFixedSize(200, 60);
    btn->setStyleSheet(
        "QPushButton{"
        " background:qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "  stop:0 #ffd700,stop:1 #ff8c00);"
        " color:#333;font-size:22px;font-weight:bold;"
        " border-radius:10px;border:2px solid #b8860b;"
        "}"
        "QPushButton:hover{"
        " background:qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "  stop:0 #ffe44d,stop:1 #ffa500);"
        "}");
    connect(btn, &QPushButton::clicked, this, &HomeScene::startGame);
    addWidget(btn)->setPos(300, 320);

    // 音乐开关
    auto *musicBtn = new QPushButton(
        udm->isMusicMuted() ? "音乐: 关" : "音乐: 开");
    musicBtn->setFixedSize(120, 36);
    musicBtn->setStyleSheet(
        "QPushButton{background:#555;color:white;border-radius:5px;}"
        "QPushButton:hover{background:#777;}");
    connect(musicBtn, &QPushButton::clicked, this, [musicBtn]() {
        auto *u = UserDataManager::getInstance();
        bool m = !u->isMusicMuted();
        u->setMusicMuted(m);
        musicBtn->setText(m ? "音乐: 关" : "音乐: 开");
    });
    addWidget(musicBtn)->setPos(250, 420);

    // 音效开关
    auto *soundBtn = new QPushButton(
        udm->isSoundMuted() ? "音效: 关" : "音效: 开");
    soundBtn->setFixedSize(120, 36);
    soundBtn->setStyleSheet(
        "QPushButton{background:#555;color:white;border-radius:5px;}"
        "QPushButton:hover{background:#777;}");
    connect(soundBtn, &QPushButton::clicked, this, [soundBtn]() {
        auto *u = UserDataManager::getInstance();
        bool m = !u->isSoundMuted();
        u->setSoundMuted(m);
        soundBtn->setText(m ? "音效: 关" : "音效: 开");
    });
    addWidget(soundBtn)->setPos(430, 420);

    // 操作说明
    auto *help = addText(
        "操作: [空格]放钩  [←→]控制方向  [B]炸药  [Esc]返回\n"
        "手势: 手部倾斜→钩子摆动  手掌张开→放钩",
        QFont("SimHei", 10));
    help->setDefaultTextColor(QColor(150, 150, 150));
    help->setPos(200, 500);
}

void HomeScene::playLogoAnimation()
{
    m_logoAnim = new QVariantAnimation(this);
    m_logoAnim->setDuration(800);
    m_logoAnim->setStartValue(0.1);
    m_logoAnim->setEndValue(1.0);
    m_logoAnim->setEasingCurve(QEasingCurve::OutBack);
    connect(m_logoAnim, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &v) { setLogoScale(v.toReal()); });
    m_logoAnim->start(QAbstractAnimation::DeleteWhenStopped);
}
