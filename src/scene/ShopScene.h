/**
 * ShopScene — 商店场景
 *
 * 四种道具购买（各限购一次），道具效果仅下一关有效
 * 参考原项目 ShopScene / goodsDesVec 设计
 */
#ifndef SHOPSCENE_H
#define SHOPSCENE_H

#include <QGraphicsScene>
#include <QGraphicsProxyWidget>
#include <QPushButton>
#include <QLabel>

class ShopScene : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit ShopScene(QObject *parent = nullptr);

    void refresh();  // 刷新金币显示

signals:
    void startGame(bool isBuyBomb, bool isBuyPotion,
                   bool isBuyDiamonds, bool isStoneBook, int payMoney);
    void backToHome();

private:
    void setupUI();
    void selectItem(int index);
    void buyItem();

    // ========== 道具数据 ==========
    struct GoodsInfo {
        QString name;
        QString description;
        int     price;
        bool    purchased;
    };
    GoodsInfo m_goods[4];

    int m_selectedIndex = -1;  // 当前选中的道具索引
    int m_payMoney      = 0;   // 累计消费金额

    // ========== UI 元素 ==========
    QGraphicsTextItem    *m_moneyText   = nullptr;
    QGraphicsTextItem    *m_descText    = nullptr;
    QGraphicsProxyWidget *m_buyProxy    = nullptr;
    QPushButton          *m_buyButton   = nullptr;
    QPushButton          *m_goodsButtons[4] = {};

    // 道具图标（标记是否已购买）
    QGraphicsProxyWidget *m_purchasedMarkers[4] = {};
};

#endif // SHOPSCENE_H
