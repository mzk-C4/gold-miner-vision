#include "Mineral.h"

Mineral::Mineral(qreal x, qreal y, int value, int weight,
                 qreal size, const QColor &color, const QString &typeName)
    : m_value(value), m_weight(weight), m_size(size),
      m_color(color), m_typeName(typeName)
{
    setPos(x, y);
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

void Mineral::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setBrush(m_color);
    painter->setPen(QPen(Qt::black, 1.5));

    qreal r = m_size;
    painter->drawEllipse(QPointF(0, 0), r, r);

    // 价值标签
    painter->setPen(Qt::white);
    int fontSize = qMax(8, int(r * 0.55));
    painter->setFont(QFont("SimHei", fontSize, QFont::Bold));
    painter->drawText(boundingRect(), Qt::AlignCenter,
                      QString("$%1").arg(m_value));
}

// ==================== 派生类构造 ====================
SmallGold::SmallGold(qreal x, qreal y, int value, int weight)
    : Mineral(x, y, value, weight, 25, QColor(255, 215, 0), "small_gold") {}

BigGold::BigGold(qreal x, qreal y, int value, int weight)
    : Mineral(x, y, value, weight, 40, QColor(255, 180, 0), "big_gold") {}

Diamond::Diamond(qreal x, qreal y, int value, int weight)
    : Mineral(x, y, value, weight, 12, QColor(100, 200, 255), "diamond") {}

Stone::Stone(qreal x, qreal y, int value, int weight)
    : Mineral(x, y, value, weight, 35, QColor(128, 128, 128), "stone") {}

// ==================== 工厂函数 ====================
Mineral* createMineralFromConfig(const QString &type, qreal x, qreal y,
                                  int value, int weight, qreal /*size*/)
{
    if (type == "small_gold")
        return new SmallGold(x, y, value, weight);
    if (type == "big_gold")
        return new BigGold(x, y, value, weight);
    if (type == "diamond")
        return new Diamond(x, y, value, weight);
    return new Stone(x, y, value, weight);
}
