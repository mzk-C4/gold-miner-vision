/**
 * Hook — 钩子系统（核心状态机）
 *
 * 5 个状态：SWINGING → EXTENDING → COLLISION → RETRACTING → SETTLING
 * 图形结构：pivot(不可见锚点) → rope → middleCircle → leftHook/rightHook
 * 摇摆动画通过 QPropertyAnimation 旋转 pivot 实现
 * 参考原项目 rope/middleCircle/leftHook/rightHook 设计
 */
#ifndef HOOK_H
#define HOOK_H

#include <QObject>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QTimer>
#include <QPointF>
#include "Mineral.h"

// ==================== 钩子状态枚举 ====================
enum class HookState {
    SWINGING,    // 左右摇摆中（初始状态）
    EXTENDING,   // 绳索沿当前角度伸长
    COLLISION,   // 碰撞判定瞬间（瞬时状态，立即转入 RETRACTING）
    RETRACTING,  // 绳索回收中（带着矿物或无物）
    SETTLING     // 完全收回后结算加分
};

class Hook : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal swingAngle READ swingAngle WRITE setSwingAngle)

public:
    explicit Hook(QGraphicsScene *scene, QObject *parent = nullptr);
    ~Hook() override;

    // ========== 初始化 ==========
    void init(const QPointF &anchorPos);   // 设置锚点并创建图形项
    void removeFromScene();                // 移除所有图形项

    // ========== 状态控制（对外接口） ==========
    void startSwinging();                  // 进入 SWINGING 状态
    void releaseHook();                    // 放钩 → EXTENDING
    void startRetracting(int speed = 10);  // 回收 → RETRACTING
    void settle();                         // 结算 → SETTLING
    void applyBomb();                      // 使用炸药（炸毁抓取物，快速回收）
    void resetToSwinging();                // 一轮完成，回到摇摆

    // ========== 钩子开合 ==========
    void openHook();                       // 钩子张开（抓住矿物）
    void closeHook();                      // 钩子闭合

    // ========== 状态查询 ==========
    HookState state() const        { return m_state; }
    bool isOpenHook() const        { return m_isOpenHook; }
    bool isRopeChanging() const    { return m_state == HookState::EXTENDING
                                          || m_state == HookState::RETRACTING; }
    qreal swingAngle() const       { return m_swingAngle; }
    void setSwingAngle(qreal angle);

    // 碰撞检测用：返回 middleCircle 供 collidingItems() 查询
    QGraphicsItem* collisionItem() const { return m_middleCircle; }

    // 抓取的矿物
    Mineral* grabbedMineral() const { return m_grabbedMineral; }
    void setGrabbedMineral(Mineral *m);

    // 道具参数设置
    void setBackSpeed(int speed)    { m_backSpeed = speed; }
    int  backSpeed() const          { return m_backSpeed; }

signals:
    void stateChanged(HookState newState);
    void grabbed(Mineral *mineral);            // 碰撞检测到矿物
    void retractComplete();                    // 回收完成
    void settlementDone();                     // 结算完成

private slots:
    void onExtendTick();                       // 伸长定时器回调
    void onRetractTick();                      // 回收定时器回调

private:
    void setState(HookState s);
    void updateRopeAndCircle();                // 更新绳索长度和圆位置

    QGraphicsScene *m_scene = nullptr;

    // ========== 图形项层级 ==========
    // pivot(旋转锚点) → rope → middleCircle → leftHook/rightHook
    QGraphicsItem      *m_pivotItem     = nullptr;  // 不可见旋转锚点
    QGraphicsRectItem  *m_ropeItem      = nullptr;  // 绳索
    QGraphicsEllipseItem *m_middleCircle = nullptr; // 中间圆（碰撞体）
    QGraphicsPathItem  *m_leftHook      = nullptr;  // 左钩爪
    QGraphicsPathItem  *m_rightHook     = nullptr;  // 右钩爪

    // ========== 动画与定时器 ==========
    QSequentialAnimationGroup *m_swingGroup = nullptr;
    QTimer *m_extendTimer  = nullptr;   // 伸长定时器（0.025s 间隔）
    QTimer *m_retractTimer = nullptr;   // 回收定时器（0.025s 间隔）

    // ========== 状态变量 ==========
    HookState m_state       = HookState::SWINGING;
    bool      m_isOpenHook  = false;     // 钩子是否已张开（抓到东西）
    qreal     m_ropeHeight  = 100.0;     // 当前绳索长度（像素）
    qreal     m_swingAngle  = 0.0;       // 当前摇摆角度（度）
    int       m_backSpeed   = 10;        // 回收步长（px/tick），默认10，药水12
    int       m_extendSpeed = 4;         // 伸长步长（px/tick）

    // ========== 锚点与位置 ==========
    QPointF m_anchorPos;                 // 世界坐标中的锚点位置
    QPointF m_circleLocalPos;            // middleCircle 在 pivot 坐标系中的位置

    // ========== 抓取 ==========
    Mineral *m_grabbedMineral = nullptr;
};

#endif // HOOK_H
