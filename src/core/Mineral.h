/**
 * Mineral — 矿物类体系（基类 + 4 种派生类）
 * 每种矿物有不同的价值、重量、尺寸和颜色
 * 继承 QGraphicsItem，支持 QGraphicsScene 碰撞检测
 */
#ifndef MINERAL_H
#define MINERAL_H

#include <QGraphicsItem>
#include <QPainter>
#include <QString>

class Mineral : public QGraphicsItem
{
public:
    Mineral(qreal x, qreal y, int value, int weight, qreal size,
            const QColor &color, const QString &typeName);

    int     value()    const { return m_value; }
    int     weight()   const { return m_weight; }
    qreal   minSize()  const { return m_size; }
    QString typeName() const { return m_typeName; }

    void setValue(int v)  { m_value = v; }
    void setWeight(int w) { m_weight = w; }

    // QGraphicsItem 接口
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;

    enum { Type = QGraphicsItem::UserType + 1 };
    int type() const override { return Type; }

protected:
    int     m_value;
    int     m_weight;
    qreal   m_size;
    QColor  m_color;
    QString m_typeName;
};

// ==================== 小金块 ====================
class SmallGold : public Mineral
{
public:
    explicit SmallGold(qreal x, qreal y, int value = 200, int weight = 5);
};

// ==================== 大金块 ====================
class BigGold : public Mineral
{
public:
    explicit BigGold(qreal x, qreal y, int value = 500, int weight = 10);
};

// ==================== 钻石 ====================
class Diamond : public Mineral
{
public:
    explicit Diamond(qreal x, qreal y, int value = 600, int weight = 1);
};

// ==================== 石头 ====================
class Stone : public Mineral
{
public:
    explicit Stone(qreal x, qreal y, int value = 20, int weight = 15);
};

// ==================== 工厂函数 ====================
Mineral* createMineralFromConfig(const QString &type, qreal x, qreal y,
                                  int value, int weight, qreal size);

#endif // MINERAL_H
