#include "SceneManager.h"
#include "HomeScene.h"
#include "ShopScene.h"
#include "GameScene.h"
#include "gesture/HandTracker.h"
#include "data/UserDataManager.h"
#include <QGraphicsView>
#include <QMessageBox>

SceneManager::SceneManager(QGraphicsView *view, QObject *parent)
    : QObject(parent), m_view(view)
{
    // 创建手势追踪器（工作线程）
    m_handTracker = new HandTracker(this);

    // 显示首页
    showHome();
}

SceneManager::~SceneManager()
{
    if (m_handTracker) {
        m_handTracker->stop();
    }
}

// ==================== 场景切换 ====================

void SceneManager::showHome()
{
    m_currentPage = HomePage;

    if (!m_homeScene) {
        m_homeScene = new HomeScene(this);
        connect(m_homeScene, &HomeScene::startGame, this, [this]() {
            // 首次游戏 → 直接进入游戏场景
            int stage = UserDataManager::getInstance()->getStageNum();
            if (stage <= 1) {
                // 没有记录，直接开始第一关
                showGame(false, false, false, false, 0);
            } else {
                // 有记录，进入商店
                showShop();
            }
        });
    }

    m_view->setScene(m_homeScene);
}

void SceneManager::showShop()
{
    m_currentPage = ShopPage;

    if (!m_shopScene) {
        m_shopScene = new ShopScene(this);
        connect(m_shopScene, &ShopScene::startGame, this, [this](
                bool bomb, bool potion, bool diamonds, bool stoneBook, int pay) {
            m_isBuyBomb     = bomb;
            m_isBuyPotion   = potion;
            m_isBuyDiamonds = diamonds;
            m_isStoneBook   = stoneBook;
            m_payMoney      = pay;
            showGame(bomb, potion, diamonds, stoneBook, pay);
        });
        connect(m_shopScene, &ShopScene::backToHome, this, &SceneManager::showHome);
    }

    m_shopScene->refresh();
    m_view->setScene(m_shopScene);
}

void SceneManager::showGame(bool isBuyBomb, bool isBuyPotion,
                             bool isBuyDiamonds, bool isStoneBook, int payMoney)
{
    m_currentPage = GamePage;

    m_isBuyBomb     = isBuyBomb;
    m_isBuyPotion   = isBuyPotion;
    m_isBuyDiamonds = isBuyDiamonds;
    m_isStoneBook   = isStoneBook;
    m_payMoney      = payMoney;

    // 删除旧场景，创建新游戏场景
    if (m_gameScene) {
        m_gameScene->deleteLater();
    }

    m_gameScene = GameScene::create(isBuyBomb, isBuyPotion,
                                     isBuyDiamonds, isStoneBook, payMoney);
    connectGameSignals(m_gameScene);

    m_view->setScene(m_gameScene);
    m_gameScene->setFocus(); // 确保接收键盘事件
}

void SceneManager::showGameFromShop()
{
    showGame(m_isBuyBomb, m_isBuyPotion, m_isBuyDiamonds, m_isStoneBook, m_payMoney);
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
        m_handTracker->start();
    }
    // AI 模式由 HandTracker 内部切换
}

// ==================== 游戏信号连接 ====================

void SceneManager::connectGameSignals(GameScene *game)
{
    connect(game, &GameScene::levelPassed, this, &SceneManager::onLevelPassed);
    connect(game, &GameScene::levelFailed, this, &SceneManager::onLevelFailed);
    connect(game, &GameScene::backToHome, this, &SceneManager::onBackToHome);

    // 连接手势信号到游戏场景
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

void SceneManager::onLevelPassed(int score)
{
    Q_UNUSED(score);
    // 过关后进入商店准备下一关
    showShop();
}

void SceneManager::onLevelFailed(int score)
{
    Q_UNUSED(score);
    // 失败后重试本关
    showGameFromShop();
}

void SceneManager::onBackToHome()
{
    // 清理游戏场景
    if (m_gameScene) {
        m_gameScene->deleteLater();
        m_gameScene = nullptr;
    }
    showHome();
}
