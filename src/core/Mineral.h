/**
 * Mineral — 矿物类体系（基类 + 4 种派生类）
 *
 * 每种矿物有不同属性：价值、重量、尺寸
 * 使用 QGraphicsItem 实现，支持碰撞检测
 * 参考原项目 plist 矿物配置
 */
#ifndef MINERAL_H
#define MINERAL_H

#include <QGraphicsItem>
#include <QPainter>
#include <QString>

// ==================== 矿物基类 ====================
class Mineral : public QGraphicsItem
{
public:
    Mineral(qreal x, qreal y, int value, int weight, qreal size,
            const QColor &color, const QString &typeName);

    // 属性访问
    int     value()    const { return m_value; }
    int     weight()   const { return m_weight; }
    qreal   minSize()  const { return m_size; }
    QString typeName() const { return m_typeName; }

    void setValue(int v) { m_value = v; }
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

// ==================== 小金块（价值中等，重量轻） ====================
class SmallGold : public Mineral
{
public:
    explicit SmallGold(qreal x, qreal y, int value = 200, int weight = 5);
};

// ==================== 大金块（价值高，重量中等） ====================
class BigGold : public Mineral
{
public:
    explicit BigGold(qreal x, qreal y, int value = 500, int weight = 10);
};

// ==================== 钻石（价值很高，重量很轻） ====================
class Diamond : public Mineral
{
public:
    explicit Diamond(qreal x, qreal y, int value = 600, int weight = 1);
};

// ==================== 石头（价值低，重量很重） ====================
class Stone : public Mineral
{
public:
    explicit Stone(qreal x, qreal y, int value = 20, int weight = 15);
};

// ==================== 工厂函数 ====================
Mineral* createMineralFromConfig(const QString &type, qreal x, qreal y,
                                  int value, int weight, qreal size);

#endif // MINERAL_H
