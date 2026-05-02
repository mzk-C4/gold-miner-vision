/**
 * ShopScene — 商店场景
 * 四种道具购买（各限购一次），道具效果仅下一关有效
 */
#ifndef SHOPSCENE_H
#define SHOPSCENE_H

#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsProxyWidget>
#include <QPushButton>
#include <QLabel>

class ShopScene : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit ShopScene(QObject *parent = nullptr);
    void refresh();

signals:
    void startGame(bool isBuyBomb, bool isBuyPotion,
                   bool isBuyDiamonds, bool isStoneBook, int payMoney);
    void backToHome();

private:
    void setupUI();
    void selectItem(int index);
    void buyItem();

    struct GoodsInfo {
        QString name;
        QString desc;
        int price = 0;
        bool purchased = false;
    };

    GoodsInfo m_goods[4];
    int m_selectedIndex = -1;
    int m_payMoney = 0;

    QGraphicsTextItem    *m_moneyText = nullptr;
    QGraphicsTextItem    *m_descText  = nullptr;
    QPushButton          *m_buyButton = nullptr;
    QPushButton          *m_goodsBtns[4] = {};
    QGraphicsProxyWidget *m_markers[4] = {};
};

#endif // SHOPSCENE_H
