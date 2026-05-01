/**
 * SceneManager — 场景管理器
 *
 * 职责：管理场景切换（首页 → 商店 → 游戏）
 * 通过 QGraphicsView::setScene() 切换当前场景
 */
#ifndef SCENEMANAGER_H
#define SCENEMANAGER_H

#include <QObject>
#include <QGraphicsView>

class QGraphicsScene;
class HomeScene;
class ShopScene;
class GameScene;
class HandTracker;

class SceneManager : public QObject
{
    Q_OBJECT

public:
    explicit SceneManager(QGraphicsView *view, QObject *parent = nullptr);
    ~SceneManager() override;

    enum Page { HomePage, ShopPage, GamePage };
    Page currentPage() const { return m_currentPage; }

    // ========== 场景切换 ==========
    void showHome();
    void showShop();
    void showGame(bool isBuyBomb, bool isBuyPotion,
                  bool isBuyDiamonds, bool isStoneBook, int payMoney);
    void showGameFromShop();

    // ========== 模式控制 ==========
    enum VisionMode { ModeKeyboard, ModeLocalCV, ModeAIVision };
    void setVisionMode(VisionMode mode);
    VisionMode visionMode() const { return m_visionMode; }

    // ========== 手势追踪器访问 ==========
    HandTracker* handTracker() const { return m_handTracker; }

private slots:
    void onLevelPassed(int score);
    void onLevelFailed(int score);
    void onBackToHome();

private:
    void connectGameSignals(GameScene *game);

    QGraphicsView *m_view       = nullptr;
    Page           m_currentPage = HomePage;
    VisionMode     m_visionMode  = ModeKeyboard;

    // 场景实例
    HomeScene *m_homeScene  = nullptr;
    ShopScene *m_shopScene  = nullptr;
    GameScene *m_gameScene  = nullptr;

    // 手势追踪器（工作线程）
    HandTracker *m_handTracker = nullptr;

    // 商店购买状态
    bool m_isBuyBomb      = false;
    bool m_isBuyPotion    = false;
    bool m_isBuyDiamonds  = false;
    bool m_isStoneBook    = false;
    int  m_payMoney       = 0;
};

#endif // SCENEMANAGER_H
