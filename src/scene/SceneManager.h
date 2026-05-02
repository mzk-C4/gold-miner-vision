/**
 * SceneManager — 场景管理器
 * 管理场景切换（首页 → 商店 → 游戏）和手势追踪器生命周期
 */
#ifndef SCENEMANAGER_H
#define SCENEMANAGER_H

#include <QObject>

class QGraphicsView;
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

    enum VisionMode { ModeKeyboard, ModeLocalCV, ModeAIVision };
    void setVisionMode(VisionMode mode);
    VisionMode visionMode() const { return m_visionMode; }

    HandTracker* handTracker() const { return m_handTracker; }

public slots:
    void showHome();
    void showShop();
    void showGame(bool isBuyBomb = false, bool isBuyPotion = false,
                  bool isBuyDiamonds = false, bool isStoneBook = false,
                  int payMoney = 0);

private slots:
    void onLevelPassed(int score);
    void onLevelFailed(int score);
    void onBackToHome();

private:
    void connectGameSignals(GameScene *game);
    void disconnectGameSignals(GameScene *game);

    QGraphicsView *m_view = nullptr;
    Page m_currentPage = HomePage;
    VisionMode m_visionMode = ModeKeyboard;

    HomeScene *m_homeScene = nullptr;
    ShopScene *m_shopScene = nullptr;
    GameScene *m_gameScene = nullptr;

    HandTracker *m_handTracker = nullptr;

    // 商店购买状态缓存
    bool m_buyBomb     = false;
    bool m_buyPotion   = false;
    bool m_buyDiamonds = false;
    bool m_buyStoneBook = false;
    int  m_payMoney    = 0;
};

#endif // SCENEMANAGER_H
