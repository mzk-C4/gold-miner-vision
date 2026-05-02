#include "Hook.h"
#include <QPainterPath>
#include <QPen>
#include <QBrush>
#include <algorithm>

Hook::Hook(QGraphicsScene *scene, QObject *parent)
    : QObject(parent), m_scene(scene)
{
    m_extendTimer = new QTimer(this);
    m_extendTimer->setInterval(25);
    connect(m_extendTimer, &QTimer::timeout, this, &Hook::onExtendTick);

    m_retractTimer = new QTimer(this);
    m_retractTimer->setInterval(25);
    connect(m_retractTimer, &QTimer::timeout, this, &Hook::onRetractTick);
}

Hook::~Hook()
{
    removeFromScene();
}

void Hook::init(const QPointF &anchorPos)
{
    m_anchorPos = anchorPos;

    // 不可见旋转锚点
    m_pivotItem = new QGraphicsRectItem(0, 0, 0, 0);
    m_pivotItem->setPos(anchorPos);
    m_pivotItem->setFlag(QGraphicsItem::ItemHasNoContents);
    m_scene->addItem(m_pivotItem);

    // 绳索
    m_ropeItem = new QGraphicsRectItem(0, 0, 3, m_ropeHeight);
    m_ropeItem->setPen(Qt::NoPen);
    m_ropeItem->setBrush(QBrush(QColor(139, 90, 43)));
    m_ropeItem->setParentItem(m_pivotItem);

    // 中间圆（碰撞检测体）
    qreal cr = 12;
    m_middleCircle = new QGraphicsEllipseItem(-cr, -cr, 2 * cr, 2 * cr);
    m_middleCircle->setPen(QPen(Qt::black, 2));
    m_middleCircle->setBrush(QBrush(QColor(180, 180, 180)));
    m_middleCircle->setParentItem(m_pivotItem);
    m_circleLocalPos = QPointF(0, m_ropeHeight);
    m_middleCircle->setPos(m_circleLocalPos);

    // 左钩爪
    QPainterPath leftPath;
    leftPath.moveTo(0, 0);
    leftPath.lineTo(-15, 25);
    leftPath.moveTo(0, 0);
    leftPath.lineTo(-5, 25);
    m_leftHook = new QGraphicsPathItem(leftPath);
    m_leftHook->setPen(QPen(Qt::black, 2));
    m_leftHook->setParentItem(m_middleCircle);

    // 右钩爪
    QPainterPath rightPath;
    rightPath.moveTo(0, 0);
    rightPath.lineTo(15, 25);
    rightPath.moveTo(0, 0);
    rightPath.lineTo(5, 25);
    m_rightHook = new QGraphicsPathItem(rightPath);
    m_rightHook->setPen(QPen(Qt::black, 2));
    m_rightHook->setParentItem(m_middleCircle);

    // 摇摆动画序列：65° → 0° → -65° → 0°，无限循环
    auto makeAnim = [this](qreal target, int dur) {
        auto *a = new QPropertyAnimation(this, "swingAngle", this);
        a->setDuration(dur);
        a->setEndValue(target);
        a->setEasingCurve(QEasingCurve::InOutSine);
        return a;
    };
    m_swingGroup = new QSequentialAnimationGroup(this);
    m_swingGroup->addAnimation(makeAnim(65, 1000));
    m_swingGroup->addAnimation(makeAnim(0, 1000));
    m_swingGroup->addAnimation(makeAnim(-65, 1000));
    m_swingGroup->addAnimation(makeAnim(0, 1000));
    m_swingGroup->setLoopCount(-1);
}

void Hook::removeFromScene()
{
    if (m_swingGroup) m_swingGroup->stop();
    m_extendTimer->stop();
    m_retractTimer->stop();
    if (m_pivotItem && m_scene) {
        m_scene->removeItem(m_pivotItem);
        delete m_pivotItem;
        m_pivotItem = nullptr;
    }
    m_ropeItem = nullptr;
    m_middleCircle = nullptr;
    m_leftHook = nullptr;
    m_rightHook = nullptr;
}

// ==================== 状态控制 ====================

void Hook::startSwinging()
{
    if (m_state == HookState::SWINGING) return;
    setState(HookState::SWINGING);
    m_swingGroup->start();
}

void Hook::releaseHook()
{
    if (m_state != HookState::SWINGING || isRopeChanging()) return;

    m_swingGroup->pause();
    m_isOpenHook = false;
    setState(HookState::EXTENDING);
    m_extendTimer->start();
}

void Hook::startRetracting(int speed)
{
    m_extendTimer->stop();
    m_backSpeed = speed;
    setState(HookState::RETRACTING);
    m_retractTimer->start();
}

void Hook::settle()
{
    m_retractTimer->stop();
    setState(HookState::SETTLING);
    emit settlementDone();
}

void Hook::applyBomb()
{
    if (!m_isOpenHook || !m_grabbedMineral) return;
    if (m_scene) {
        m_scene->removeItem(m_grabbedMineral);
        delete m_grabbedMineral;
        m_grabbedMineral = nullptr;
    }
    closeHook();
    startRetracting(15);
}

void Hook::resetToSwinging()
{
    closeHook();
    m_grabbedMineral = nullptr;
    m_ropeHeight = 100.0;
    m_backSpeed = 10;
    updateRopeAndCircle();
    startSwinging();
}

// ==================== 钩子开合 ====================

void Hook::openHook()
{
    m_isOpenHook = true;
    m_leftHook->setRotation(-30);
    m_rightHook->setRotation(30);
}

void Hook::closeHook()
{
    m_isOpenHook = false;
    m_leftHook->setRotation(0);
    m_rightHook->setRotation(0);
}

// ==================== 摇摆角度 ====================

void Hook::setSwingAngle(qreal angle)
{
    m_swingAngle = angle;
    if (m_pivotItem)
        m_pivotItem->setRotation(angle);
}

void Hook::setGrabbedMineral(Mineral *m)
{
    m_grabbedMineral = m;
}

// ==================== 伸长回调 ====================

void Hook::onExtendTick()
{
    if (m_state != HookState::EXTENDING) return;

    m_ropeHeight += m_extendSpeed;
    updateRopeAndCircle();

    // 触底 → 快速回收
    if (m_ropeHeight >= 550) {
        startRetracting(15);
        return;
    }

    // 碰撞检测
    checkCollision();
}

// ==================== 回收回调 ====================

void Hook::onRetractTick()
{
    if (m_state != HookState::RETRACTING) return;

    m_ropeHeight -= m_backSpeed;

    if (m_ropeHeight <= 20) {
        m_ropeHeight = 20;
        updateRopeAndCircle();
        m_retractTimer->stop();
        emit retractComplete();
        return;
    }

    updateRopeAndCircle();

    // 矿物跟随 middleCircle
    if (m_grabbedMineral && m_middleCircle) {
        QPointF wp = m_middleCircle->mapToScene(0, 0);
        m_grabbedMineral->setPos(wp);
    }
}

// ==================== 碰撞检测 ====================

void Hook::checkCollision()
{
    if (!m_scene || !m_middleCircle) return;

    QList<QGraphicsItem*> items = m_scene->collidingItems(m_middleCircle);
    for (auto *item : items) {
        auto *mineral = dynamic_cast<Mineral*>(item);
        if (mineral) {
            m_extendTimer->stop();
            setGrabbedMineral(mineral);
            openHook();
            setState(HookState::COLLISION);
            emit grabbed(mineral);
            startRetracting(m_backSpeed);
            return;
        }
    }
}

// ==================== 辅助方法 ====================

void Hook::setState(HookState s)
{
    if (m_state != s) {
        m_state = s;
        emit stateChanged(s);
    }
}

void Hook::updateRopeAndCircle()
{
    if (m_ropeItem)
        m_ropeItem->setRect(0, 0, 3, m_ropeHeight);
    m_circleLocalPos = QPointF(0, m_ropeHeight);
    if (m_middleCircle)
        m_middleCircle->setPos(m_circleLocalPos);
}
