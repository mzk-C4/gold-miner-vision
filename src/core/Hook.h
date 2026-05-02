/**
 * Hook — 钩子系统（核心状态机）
 *
 * 状态：SWINGING → EXTENDING → COLLISION → RETRACTING → SETTLING → SWINGING
 * 图形结构：pivot(旋转锚点) → rope(绳索) → middleCircle(碰撞体) → leftHook/rightHook(钩爪)
 * 摇摆：QPropertyAnimation 旋转 pivot 实现
 * 伸缩：QTimer 驱动，参考原项目 addRopeHeight / subRopeHeight
 */
#ifndef HOOK_H
#define HOOK_H

#include <QObject>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QTimer>
#include "Mineral.h"

enum class HookState {
    SWINGING,
    EXTENDING,
    COLLISION,
    RETRACTING,
    SETTLING
};

class Hook : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal swingAngle READ swingAngle WRITE setSwingAngle)

public:
    explicit Hook(QGraphicsScene *scene, QObject *parent = nullptr);
    ~Hook() override;

    // ========== 初始化与清理 ==========
    void init(const QPointF &anchorPos);
    void removeFromScene();

    // ========== 状态控制 ==========
    void startSwinging();
    void releaseHook();
    void startRetracting(int speed = 10);
    void settle();
    void applyBomb();
    void resetToSwinging();

    // ========== 钩子开合 ==========
    void openHook();
    void closeHook();

    // ========== 状态查询 ==========
    HookState state() const      { return m_state; }
    bool isOpenHook() const      { return m_isOpenHook; }
    bool isRopeChanging() const  { return m_state == HookState::EXTENDING
                                        || m_state == HookState::RETRACTING; }
    qreal swingAngle() const     { return m_swingAngle; }
    void  setSwingAngle(qreal angle);

    QGraphicsItem* collisionItem() const { return m_middleCircle; }
    Mineral* grabbedMineral() const      { return m_grabbedMineral; }
    void setGrabbedMineral(Mineral *m);

    void setBackSpeed(int speed) { m_backSpeed = speed; }
    int  backSpeed() const       { return m_backSpeed; }

signals:
    void stateChanged(HookState newState);
    void grabbed(Mineral *mineral);
    void retractComplete();
    void settlementDone();

private slots:
    void onExtendTick();
    void onRetractTick();

private:
    void setState(HookState s);
    void updateRopeAndCircle();
    void checkCollision();

    QGraphicsScene *m_scene = nullptr;

    // 图形项层级：pivot → rope / middleCircle → hooks
    QGraphicsRectItem    *m_pivotItem    = nullptr;
    QGraphicsRectItem    *m_ropeItem     = nullptr;
    QGraphicsEllipseItem *m_middleCircle = nullptr;
    QGraphicsPathItem    *m_leftHook     = nullptr;
    QGraphicsPathItem    *m_rightHook    = nullptr;

    // 动画与定时器
    QSequentialAnimationGroup *m_swingGroup   = nullptr;
    QTimer *m_extendTimer  = nullptr;
    QTimer *m_retractTimer = nullptr;

    // 状态
    HookState m_state       = HookState::SWINGING;
    bool      m_isOpenHook  = false;
    qreal     m_ropeHeight  = 100.0;
    qreal     m_swingAngle  = 0.0;
    int       m_backSpeed   = 10;
    int       m_extendSpeed = 4;

    // 位置
    QPointF m_anchorPos;
    QPointF m_circleLocalPos;

    Mineral *m_grabbedMineral = nullptr;
};

#endif // HOOK_H
