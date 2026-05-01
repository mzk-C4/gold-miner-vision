#include "Mineral.h"
#include <cmath>

// ==================== 基类实现 ====================
Mineral::Mineral(qreal x, qreal y, int value, int weight,
                 qreal size, const QColor &color, const QString &typeName)
    : m_value(value), m_weight(weight), m_size(size),
      m_color(color), m_typeName(typeName)
{
    setPos(x, y);
    setFlag(QGraphicsItem::ItemIsMovable, false);
}

QRectF Mineral::boundingRect() const
{
    qreal r = m_size;
    return QRectF(-r, -r, 2 * r, 2 * r);
}

QPainterPath Mineral::shape() const
{
    QPainterPath path;
    path.addEllipse(boundingRect());
    return path;
}

void Mineral::paint(QPainter *painter, const QStyleOptionGraphicsItem *,
                    QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setBrush(m_color);
    painter->setPen(Qt::black);

    qreal r = m_size;
    // 画圆形矿物
    painter->drawEllipse(QPointF(0, 0), r, r);

    // 画价值文本（简化为首字母）
    painter->setPen(Qt::white);
    painter->setFont(QFont("Arial", int(r * 0.6), QFont::Bold));
    QString label;
    if (m_typeName == "small_gold")  label = "金";
    else if (m_typeName == "big_gold")  label = "G";
    else if (m_typeName == "diamond")   label = "钻";
    else if (m_typeName == "stone")     label = "石";
    painter->drawText(boundingRect(), Qt::AlignCenter, label);
}

// ==================== 小金块 ====================
SmallGold::SmallGold(qreal x, qreal y, int value, int weight)
    : Mineral(x, y, value, weight, 25, QColor(255, 215, 0), "small_gold")
{}

// ==================== 大金块 ====================
BigGold::BigGold(qreal x, qreal y, int value, int weight)
    : Mineral(x, y, value, weight, 40, QColor(255, 180, 0), "big_gold")
{}

// ==================== 钻石 ====================
Diamond::Diamond(qreal x, qreal y, int value, int weight)
    : Mineral(x, y, value, weight, 12, QColor(100, 200, 255), "diamond")
{}

// ==================== 石头 ====================
Stone::Stone(qreal x, qreal y, int value, int weight)
    : Mineral(x, y, value, weight, 35, QColor(128, 128, 128), "stone")
{}

// ==================== 工厂函数 ====================
Mineral* createMineralFromConfig(const QString &type, qreal x, qreal y,
                                  int value, int weight, qreal size)
{
    if (type == "small_gold")
        return new SmallGold(x, y, value > 0 ? value : 200, weight > 0 ? weight : 5);
    else if (type == "big_gold")
        return new BigGold(x, y, value > 0 ? value : 500, weight > 0 ? weight : 10);
    else if (type == "diamond")
        return new Diamond(x, y, value > 0 ? value : 600, weight > 0 ? weight : 1);
    else // stone
        return new Stone(x, y, value > 0 ? value : 20, weight > 0 ? weight : 15);
}
