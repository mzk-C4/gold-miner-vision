#include "ShopScene.h"
#include "data/UserDataManager.h"
#include <QFont>
#include <QBrush>
#include <QVBoxLayout>

ShopScene::ShopScene(QObject *parent)
    : QGraphicsScene(parent)
{
    setSceneRect(0, 0, 800, 600);
    setBackgroundBrush(QBrush(QColor(40, 40, 70)));

    // 初始化道具数据
    m_goods[0] = {"炸药", "炸毁当前抓取的不想要矿物，节省时间。\n价格: $150（仅可购买一次）", 150, false};
    m_goods[1] = {"力量药水", "回收速度增加20%，更快完成回收。\n价格: $200（仅可购买一次）", 200, false};
    m_goods[2] = {"钻石升值书", "下一关钻石价值变为原价3倍。\n价格: $300（仅可购买一次）", 300, false};
    m_goods[3] = {"矿石收藏书", "下一关所有矿石价值变为原价3倍。\n价格: $400（仅可购买一次）", 400, false};

    setupUI();
}

void ShopScene::refresh()
{
    int money = UserDataManager::getInstance()->getAllMoney() - m_payMoney;
    if (m_moneyText) {
        m_moneyText->setPlainText(QString("金币: $%1").arg(qMax(0, money)));
    }
}

void ShopScene::setupUI()
{
    QFont titleFont("SimHei", 28, QFont::Bold);
    QFont descFont("Arial", 12);

    // 标题
    auto *title = addText("商店", titleFont);
    title->setDefaultTextColor(QColor(255, 215, 0));
    title->setPos(350, 20);

    // 金币显示
    int money = UserDataManager::getInstance()->getAllMoney();
    m_moneyText = addText(QString("金币: $%1").arg(money), QFont("Arial", 18, QFont::Bold));
    m_moneyText->setDefaultTextColor(Qt::white);
    m_moneyText->setPos(580, 25);

    // ========== 四个道具按钮 ==========
    QString btnStyle =
        "QPushButton { background: #555; color: white; font-size: 14px; "
        "border: 2px solid #888; border-radius: 8px; padding: 10px; }"
        "QPushButton:hover { background: #777; }"
        "QPushButton:checked { background: #2a6496; border-color: #4a9eff; }";

    QString names[] = {"炸药", "力量药水", "钻石升值书", "矿石收藏书"};
    for (int i = 0; i < 4; ++i) {
        auto *btn = new QPushButton(names[i]);
        btn->setFixedSize(140, 50);
        btn->setStyleSheet(btnStyle);
        btn->setCheckable(true);
        int index = i;
        connect(btn, &QPushButton::clicked, this, [this, index]() {
            selectItem(index);
        });

        m_goodsButtons[i] = btn;
        auto *proxy = addWidget(btn);
        proxy->setPos(40 + i * 185, 100);

        // 已购买标记（初始隐藏）
        auto *marker = new QLabel("1");
        marker->setFixedSize(30, 30);
        marker->setStyleSheet(
            "QLabel { background: #ff4444; color: white; font-size: 16px; "
            "font-weight: bold; border-radius: 15px; }");
        marker->setAlignment(Qt::AlignCenter);
        marker->setVisible(false);
        auto *markerProxy = addWidget(marker);
        markerProxy->setPos(150 + i * 185, 90);
        markerProxy->setZValue(10);
        m_purchasedMarkers[i] = markerProxy;
    }

    // ========== 道具描述 ==========
    m_descText = addText("选择一个道具查看描述", descFont);
    m_descText->setDefaultTextColor(QColor(200, 200, 200));
    m_descText->setPos(60, 190);

    // ========== 购买按钮 ==========
    auto *buyBtn = new QPushButton("购买");
    buyBtn->setFixedSize(150, 50);
    buyBtn->setStyleSheet(
        "QPushButton { background: #4caf50; color: white; font-size: 18px; "
        "font-weight: bold; border-radius: 8px; }"
        "QPushButton:hover { background: #5cbf60; }"
        "QPushButton:disabled { background: #666; color: #999; }");
    connect(buyBtn, &QPushButton::clicked, this, &ShopScene::buyItem);

    m_buyButton = buyBtn;
    m_buyProxy = addWidget(buyBtn);
    m_buyProxy->setPos(280, 360);

    // ========== 开始游戏按钮 ==========
    auto *startBtn = new QPushButton("开始游戏 →");
    startBtn->setFixedSize(200, 60);
    startBtn->setStyleSheet(
        "QPushButton { background: #ff8c00; color: white; font-size: 20px; "
        "font-weight: bold; border-radius: 10px; }"
        "QPushButton:hover { background: #ffa500; }");
    connect(startBtn, &QPushButton::clicked, this, [this]() {
        emit startGame(
            m_goods[0].purchased, m_goods[1].purchased,
            m_goods[2].purchased, m_goods[3].purchased, m_payMoney);
    });

    auto *startProxy = addWidget(startBtn);
    startProxy->setPos(300, 450);

    // ========== 返回按钮 ==========
    auto *backBtn = new QPushButton("返回首页");
    backBtn->setFixedSize(120, 36);
    backBtn->setStyleSheet(
        "QPushButton { background: #666; color: white; border-radius: 5px; }"
        "QPushButton:hover { background: #888; }");
    connect(backBtn, &QPushButton::clicked, this, &ShopScene::backToHome);
    auto *backProxy = addWidget(backBtn);
    backProxy->setPos(20, 550);
}

void ShopScene::selectItem(int index)
{
    if (index < 0 || index >= 4) return;

    m_selectedIndex = index;

    // 取消其他按钮的选中状态
    for (int i = 0; i < 4; ++i) {
        if (i != index && m_goodsButtons[i]) {
            m_goodsButtons[i]->setChecked(false);
        }
    }

    // 更新描述
    if (m_descText) {
        QString desc = m_goods[index].description;
        if (m_goods[index].purchased) {
            desc += "\n[已购买]";
        }
        m_descText->setPlainText(desc);
    }

    // 已购买则禁用购买按钮
    if (m_buyButton) {
        m_buyButton->setEnabled(!m_goods[index].purchased);
    }
}

void ShopScene::buyItem()
{
    if (m_selectedIndex < 0 || m_selectedIndex >= 4) return;
    if (m_goods[m_selectedIndex].purchased) return;

    int price = m_goods[m_selectedIndex].price;
    auto *udm = UserDataManager::getInstance();

    if (udm->getAllMoney() - m_payMoney >= price) {
        m_payMoney += price;
        m_goods[m_selectedIndex].purchased = true;

        // 显示已购买标记
        if (m_purchasedMarkers[m_selectedIndex]) {
            m_purchasedMarkers[m_selectedIndex]->setVisible(true);
        }

        // 更新描述
        if (m_descText) {
            m_descText->setPlainText(
                m_goods[m_selectedIndex].description + "\n[已购买]");
        }
        if (m_buyButton) {
            m_buyButton->setEnabled(false);
        }

        refresh();
    }
}
