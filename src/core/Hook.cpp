#include "Hook.h"
#include <QPainterPath>
#include <QPen>
#include <QBrush>
#include <QtMath>
#include <QGraphicsScene>

// ==================== 构造函数 ====================
Hook::Hook(QGraphicsScene *scene, QObject *parent)
    : QObject(parent), m_scene(scene)
{
    // 创建伸长定时器（每 0.025 秒触发一次 = 40fps）
    m_extendTimer = new QTimer(this);
    m_extendTimer->setInterval(25);
    connect(m_extendTimer, &QTimer::timeout, this, &Hook::onExtendTick);

    // 创建回收定时器
    m_retractTimer = new QTimer(this);
    m_retractTimer->setInterval(25);
    connect(m_retractTimer, &QTimer::timeout, this, &Hook::onRetractTick);
}

Hook::~Hook()
{
    removeFromScene();
}

// ==================== 初始化：创建图形项并添加到场景 ====================
void Hook::init(const QPointF &anchorPos)
{
    m_anchorPos = anchorPos;

    // ---- 创建不可见的旋转锚点（pivot） ----
    m_pivotItem = new QGraphicsRectItem(0, 0, 0, 0);
    m_pivotItem->setPos(anchorPos);
    m_pivotItem->setFlag(QGraphicsItem::ItemHasNoContents); // 不可见
    m_scene->addItem(m_pivotItem);

    // ---- 创建绳索（从锚点向下延伸的细矩形） ----
    m_ropeItem = new QGraphicsRectItem(0, 0, 3, m_ropeHeight);
    m_ropeItem->setPen(Qt::NoPen);
    m_ropeItem->setBrush(QBrush(QColor(139, 90, 43))); // 棕色绳索
    m_ropeItem->setParentItem(m_pivotItem);
    m_ropeItem->setPos(0, 0);

    // ---- 创建中间圆（碰撞检测体，位于绳索末端） ----
    qreal circleRadius = 12;
    m_middleCircle = new QGraphicsEllipseItem(
        -circleRadius, -circleRadius,
        2 * circleRadius, 2 * circleRadius);
    m_middleCircle->setPen(QPen(Qt::black, 2));
    m_middleCircle->setBrush(QBrush(QColor(180, 180, 180))); // 灰色金属
    m_middleCircle->setParentItem(m_pivotItem);
    m_circleLocalPos = QPointF(0, m_ropeHeight);
    m_middleCircle->setPos(m_circleLocalPos);

    // ---- 创建左钩爪（V 形） ----
    QPainterPath leftPath;
    leftPath.moveTo(0, 0);
    leftPath.lineTo(-15, 25);
    leftPath.moveTo(0, 0);
    leftPath.lineTo(-5, 25);
    m_leftHook = new QGraphicsPathItem(leftPath);
    m_leftHook->setPen(QPen(Qt::black, 2));
    m_leftHook->setParentItem(m_middleCircle);
    m_leftHook->setPos(0, 0);

    // ---- 创建右钩爪（V 形） ----
    QPainterPath rightPath;
    rightPath.moveTo(0, 0);
    rightPath.lineTo(15, 25);
    rightPath.moveTo(0, 0);
    rightPath.lineTo(5, 25);
    m_rightHook = new QGraphicsPathItem(rightPath);
    m_rightHook->setPen(QPen(Qt::black, 2));
    m_rightHook->setParentItem(m_middleCircle);
    m_rightHook->setPos(0, 0);

    // ---- 创建摇摆动画序列 ----
    // Rotate(1s, 65°) → Rotate(1s, 0°) → Rotate(1s, -65°) → Rotate(1s, 0°)
    auto makeAnim = [this](qreal target, int duration) {
        auto *anim = new QPropertyAnimation(this, "swingAngle", this);
        anim->setDuration(duration);
        anim->setEndValue(target);
        anim->setEasingCurve(QEasingCurve::InOutSine);
        return anim;
    };

    m_swingGroup = new QSequentialAnimationGroup(this);
    m_swingGroup->addAnimation(makeAnim(65, 1000));
    m_swingGroup->addAnimation(makeAnim(0, 1000));
    m_swingGroup->addAnimation(makeAnim(-65, 1000));
    m_swingGroup->addAnimation(makeAnim(0, 1000));
    m_swingGroup->setLoopCount(-1); // 无限循环
}

// ==================== 从场景移除图形项 ====================
void Hook::removeFromScene()
{
    if (m_swingGroup) {
        m_swingGroup->stop();
    }
    m_extendTimer->stop();
    m_retractTimer->stop();

    // 删除 pivot 会自动级联删除所有子项
    if (m_pivotItem && m_scene) {
        m_scene->removeItem(m_pivotItem);
        delete m_pivotItem;
        m_pivotItem = nullptr;
    }
    m_ropeItem      = nullptr;
    m_middleCircle  = nullptr;
    m_leftHook      = nullptr;
    m_rightHook     = nullptr;
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
    if (m_state != HookState::SWINGING) return;
    if (isRopeChanging()) return;

    // 暂停摇摆动画
    m_swingGroup->pause();
    m_isOpenHook = false;

    // 进入伸长状态
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
    // 结算完成后自动回到摇摆
    emit settlementDone();
}

void Hook::applyBomb()
{
    if (!m_isOpenHook || m_grabbedMineral == nullptr) return;

    // 移除抓取的矿物
    if (m_scene && m_grabbedMineral) {
        m_scene->removeItem(m_grabbedMineral);
        delete m_grabbedMineral;
        m_grabbedMineral = nullptr;
    }

    // 闭合钩子，快速回收
    closeHook();
    startRetracting(15); // 炸药快速回收
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
    // 旋转钩爪模拟"抓住"动作
    m_leftHook->setRotation(-30);
    m_rightHook->setRotation(30);
}

void Hook::closeHook()
{
    m_isOpenHook = false;
    m_leftHook->setRotation(0);
    m_rightHook->setRotation(0);
}

// ==================== 摇摆角度属性 ====================

void Hook::setSwingAngle(qreal angle)
{
    m_swingAngle = angle;
    if (m_pivotItem) {
        m_pivotItem->setRotation(angle);
    }
}

void Hook::setGrabbedMineral(Mineral *m)
{
    m_grabbedMineral = m;
}

// ==================== 伸长定时器回调 ====================

void Hook::onExtendTick()
{
    if (m_state != HookState::EXTENDING) return;

    m_ropeHeight += m_extendSpeed;
    updateRopeAndCircle();

    // 检查是否超出场景边界（绳索最大长度）
    qreal maxRope = 550; // 最大绳索长度
    if (m_ropeHeight >= maxRope) {
        startRetracting(15); // 触底快速回收
        return;
    }

    // 碰撞检测：检查 middleCircle 是否碰到矿物
    if (m_scene) {
        QList<QGraphicsItem*> colliding = m_scene->collidingItems(m_middleCircle);
        for (auto *item : colliding) {
            auto *mineral = dynamic_cast<Mineral*>(item);
            if (mineral && mineral != m_grabbedMineral) {
                // 碰撞到矿物
                m_extendTimer->stop();
                setGrabbedMineral(mineral);
                openHook();
                setState(HookState::COLLISION);
                emit grabbed(mineral);

                // 瞬间进入回收状态
                startRetracting(m_backSpeed);
                return;
            }
        }
    }
}

// ==================== 回收定时器回调 ====================

void Hook::onRetractTick()
{
    if (m_state != HookState::RETRACTING) return;

    m_ropeHeight -= m_backSpeed;

    if (m_ropeHeight <= 20) {
        // 绳索完全收回
        m_ropeHeight = 20;
        updateRopeAndCircle();
        m_retractTimer->stop();
        emit retractComplete();
        return;
    }

    updateRopeAndCircle();

    // 如果抓着矿物，矿物跟随 middleCircle 移动
    if (m_grabbedMineral && m_middleCircle) {
        QPointF worldPos = m_middleCircle->mapToScene(0, 0);
        m_grabbedMineral->setPos(worldPos);
    }
}

// ==================== 私有辅助 ====================

void Hook::setState(HookState s)
{
    if (m_state != s) {
        m_state = s;
        emit stateChanged(s);
    }
}

void Hook::updateRopeAndCircle()
{
    if (m_ropeItem) {
        m_ropeItem->setRect(0, 0, 3, m_ropeHeight);
    }
    m_circleLocalPos = QPointF(0, m_ropeHeight);
    if (m_middleCircle) {
        m_middleCircle->setPos(m_circleLocalPos);
    }
}
