#include "SceneManager.h"
#include "HomeScene.h"
#include "ShopScene.h"
#include "GameScene.h"
#include "gesture/HandTracker.h"
#include "data/UserDataManager.h"
#include <QGraphicsView>

SceneManager::SceneManager(QGraphicsView *view, QObject *parent)
    : QObject(parent), m_view(view)
{
    m_handTracker = new HandTracker(this);
    showHome();
}

SceneManager::~SceneManager()
{
    m_handTracker->stop();
}

// ==================== 场景切换 ====================

void SceneManager::showHome()
{
    m_currentPage = HomePage;

    if (!m_homeScene) {
        m_homeScene = new HomeScene(this);
        connect(m_homeScene, &HomeScene::startGame, this, [this]() {
            if (UserDataManager::getInstance()->getStageNum() <= 1)
                showGame();
            else
                showShop();
        });
    }

    m_view->setScene(m_homeScene);
}

void SceneManager::showShop()
{
    m_currentPage = ShopPage;

    if (!m_shopScene) {
        m_shopScene = new ShopScene(this);
        connect(m_shopScene, &ShopScene::startGame, this,
                [this](bool bomb, bool potion, bool diamond, bool book, int pay) {
            m_buyBomb     = bomb;
            m_buyPotion   = potion;
            m_buyDiamonds = diamond;
            m_buyStoneBook = book;
            m_payMoney    = pay;
            showGame(bomb, potion, diamond, book, pay);
        });
        connect(m_shopScene, &ShopScene::backToHome, this, &SceneManager::showHome);
    }

    m_shopScene->refresh();
    m_view->setScene(m_shopScene);
}

void SceneManager::showGame(bool bomb, bool potion, bool diamonds,
                             bool stoneBook, int pay)
{
    m_currentPage = GamePage;

    // 清理旧游戏场景
    if (m_gameScene) {
        disconnectGameSignals(m_gameScene);
        m_gameScene->deleteLater();
        m_gameScene = nullptr;
    }

    m_gameScene = GameScene::create(bomb, potion, diamonds, stoneBook, pay);
    connectGameSignals(m_gameScene);

    m_view->setScene(m_gameScene);
    m_gameScene->setFocus();
}

// ==================== 模式控制 ====================

void SceneManager::setVisionMode(VisionMode mode)
{
    m_visionMode = mode;

    if (mode == ModeKeyboard) {
        m_handTracker->stop();
        return;
    }

    if (mode == ModeLocalCV) {
        m_handTracker->setMode(HandTracker::LocalCV);
        m_handTracker->start();
    } else if (mode == ModeAIVision) {
        m_handTracker->setMode(HandTracker::AIVision);
        m_handTracker->start();
    }
}

// ==================== 游戏信号 ====================

void SceneManager::connectGameSignals(GameScene *game)
{
    connect(game, &GameScene::levelPassed, this, &SceneManager::onLevelPassed);
    connect(game, &GameScene::levelFailed, this, &SceneManager::onLevelFailed);
    connect(game, &GameScene::backToHome, this, &SceneManager::onBackToHome);

    if (m_handTracker) {
        connect(m_handTracker, &HandTracker::handTilt,
                game, &GameScene::onHandTilt);
        connect(m_handTracker, &HandTracker::handOpen,
                game, &GameScene::onHandOpen);
        connect(m_handTracker, &HandTracker::handFist,
                game, &GameScene::onHandFist);
        connect(m_handTracker, &HandTracker::handGesture,
                game, &GameScene::onHandGesture);
    }
}

void SceneManager::disconnectGameSignals(GameScene *game)
{
    disconnect(game, nullptr, this, nullptr);
    if (m_handTracker)
        disconnect(m_handTracker, nullptr, game, nullptr);
}

void SceneManager::onLevelPassed(int)
{
    showShop();
}

void SceneManager::onLevelFailed(int)
{
    // 失败重试：使用相同道具状态
    showGame(m_buyBomb, m_buyPotion, m_buyDiamonds, m_buyStoneBook, m_payMoney);
}

void SceneManager::onBackToHome()
{
    if (m_gameScene) {
        disconnectGameSignals(m_gameScene);
        m_gameScene->deleteLater();
        m_gameScene = nullptr;
    }
    showHome();
}
