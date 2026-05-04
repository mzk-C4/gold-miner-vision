//
//  MainRoot.cpp
//  GoldMiner
//
//  Created by MacBook on 16/11/22.
//
//

#include "MainRoot.hpp"
#include "Game.hpp"
#include "UserDataManager.hpp"
#include "Shop.hpp"
#include "SoundTool.hpp"
#include "MusicPlayer.hpp"
#include "Const.hpp"
#include "services/PlayerManager.hpp"

// Create a button with predictable, clean appearance — no black edges
static Button* makeCleanButton(const std::string& title, const Size& size, float fontSize)
{
    auto btn = Button::create("Resources/green.png");
    btn->setScale9Enabled(true);
    btn->setCapInsets(Rect(8, 4, 137, 46));
    btn->setContentSize(size);
    btn->setTitleText(title);
    btn->setTitleFontSize(fontSize);
    btn->setTitleColor(Color3B::BLACK);
    return btn;
}

Scene *MainLayer::createScene()
{
    Scene *scene = Scene::create();
    
    auto layer = MainLayer::create();
    scene->addChild(layer);
    
    return scene;
}

bool MainLayer::init()
{
    if (!Layer::init()) {
        return false;
    }
    
    auto mainCsb = CSLoader::createNode("Layer.csb");
    animation = CSLoader::createTimeline("Layer.csb");
    mainCsb->runAction(animation);
    addChild(mainCsb);
    
    cloud1 = static_cast<ImageView *>(Helper::seekWidgetByName(static_cast<Widget *>(mainCsb), "cloud1"));
    cloud2 = static_cast<ImageView *>(Helper::seekWidgetByName(static_cast<Widget *>(mainCsb), "cloud2"));
    
    SpriteFrameCache::getInstance()->addSpriteFramesWithFile("Resources/general-sheet.plist");
    SpriteBatchNode *batchNode = SpriteBatchNode::create("Resources/general-sheet.png");
    addChild(batchNode);
    
    leftLeg = Sprite::createWithSpriteFrameName("miner-leg-0.png");
    leftLeg->setPosition(kWinSizeWidth * 0.1579, kWinSizeHeight * 0.1613);
    batchNode->addChild(leftLeg, 10);
    
    face = Sprite::createWithSpriteFrameName("miner-face-whistle-0.png");
    face->setPosition(kWinSizeWidth * 0.2174, kWinSizeHeight * 0.5965);
    batchNode->addChild(face);
    
    light = Sprite::createWithSpriteFrameName("cave-0.png");
    light->setScale(0.5f);
    light->setPosition(Vec2(kWinSizeWidth * 0.7083, kWinSizeHeight * 0.445));
    batchNode->addChild(light);
    
    // add button Action
    _startBtn = static_cast<Button *>(Helper::seekWidgetByName(static_cast<Widget *>(mainCsb), "startButton"));
    _startBtn->addTouchEventListener(CC_CALLBACK_2(MainLayer::startButtonTouch, this));
    _startBtn->setEnabled(false);
    _startBtn->setBright(false);

    // Player button — opens combined player panel
    _playerBtn = Button::create("Resources/menu-player-gold.png");
    _playerBtn->setPosition(Vec2(kWinSizeWidth * 0.85f, kWinSizeHeight * 0.55f));
    _playerBtn->setTitleText("Player: --");
    _playerBtn->setTitleFontSize(24);
    _playerBtn->setTitleColor(Color3B::BLACK);
    _playerBtn->setScale9Enabled(true);
    _playerBtn->setContentSize(Size(400, 120));
    _playerBtn->addTouchEventListener([this](Ref*, Widget::TouchEventType type) {
        if (type == Widget::TouchEventType::ENDED) {
            kPlayClickSound
            showPlayerPanel();
        }
    });
    addChild(_playerBtn, 10);

    // Mode selection buttons at bottom-left
    auto makeModeBtn = [&](const std::string& text, int mode, float x, float y) -> Button* {
        auto btn = makeCleanButton(text, Size(140, 56), 18);
        btn->setPosition(Vec2(x, y));
        btn->addTouchEventListener([this, mode](Ref*, Widget::TouchEventType type) {
            if (type == Widget::TouchEventType::ENDED)
                onModeBtnTouch(mode);
        });
        return btn;
    };

    float mx = kWinSizeWidth * 0.15f;
    float my = kWinSizeHeight * 0.08f;
    _touchModeBtn = makeModeBtn("TOUCH", 0, mx, my);
    addChild(_touchModeBtn, 10);

    _opencvModeBtn = makeModeBtn("OPENCV", 1, mx + 160, my);
    addChild(_opencvModeBtn, 10);

    _aiModeBtn = makeModeBtn("AI", 2, mx + 320, my);
    addChild(_aiModeBtn, 10);

    _selectedMode = 0;
    if (_touchModeBtn) _touchModeBtn->setBright(true);
    if (_opencvModeBtn) _opencvModeBtn->setBright(false);
    if (_aiModeBtn) _aiModeBtn->setBright(false);

    updatePlayerLabel();

    return true;
}

void MainLayer::onEnter()
{
    Layer::onEnter();

    // playAnimation
    animation->gotoFrameAndPlay(0, 25, false);

    // 播放云动画
    moveCloud(cloud1, 10, 20);
    moveCloud(cloud2, 30, 15);
    
    // 播放吹Miner动画
    this->playMinerAnimation(leftLeg, "miner-leg-", 0.15);
    this->playMinerAnimation(face, "miner-face-whistle-", 0.25);
    this->playMinerAnimation(light, "cave-", 0.35);

    MusicPlayer::getInstance()->start();
}

void MainLayer::startButtonTouch(cocos2d::Ref *sender, Widget::TouchEventType type)
{
    if (type == Widget::TouchEventType::ENDED) {
        kPlayClickSound

        // Set the input mode for the game
        InputMode mode = InputMode::TOUCH;
        if (_selectedMode == 1) mode = InputMode::OPENCV;
        else if (_selectedMode == 2) mode = InputMode::AI;
        Game::setDefaultInputMode(mode);

        // Fresh character by default; use saved data only if explicitly selected
        if (!_playerExplicitlySelected) {
            // Switch to Guest profile so we don't overwrite saved player data
            auto* pm = PlayerManager::getInstance();
            if (pm->currentPlayer() != "Guest") {
                auto players = pm->listPlayers();
                bool hasGuest = false;
                for (const auto& p : players) {
                    if (p == "Guest") { hasGuest = true; break; }
                }
                if (!hasGuest) pm->createPlayer("Guest");
                else pm->selectPlayer("Guest");
            }
            pm->setStageNum(1);
            pm->setAllMoney(0);
            UserDataManager::getInstance()->setStageNum(1);
            UserDataManager::getInstance()->setAllMoney(0);
        }

        if (UserDataManager::getInstance()->getStageNum() <= 1) {
            auto gameScene = Game::createScene(false, false, false, false, 0);
            Director::getInstance()->replaceScene(gameScene);
        } else {
            auto shopScene = Shop::createScene();
            Director::getInstance()->replaceScene(shopScene);
        }
    }
}

void MainLayer::playMinerAnimation(cocos2d::Sprite *sprite, string imageName, float frameDelat)
{
    Vector<SpriteFrame *> frames;
    
    // 播放抖腿动画
    for (int i = 0; i < 3; i++) {
        auto frameName = imageName + to_string(i) + ".png";
        SpriteFrame *sf = SpriteFrameCache::getInstance()->getSpriteFrameByName(frameName);
        frames.pushBack(sf);
    }
    
    Animation *shake = Animation::createWithSpriteFrames(frames);
    shake->setDelayPerUnit(frameDelat);
    shake->setRestoreOriginalFrame(true);
    Animate *shakeAn = Animate::create(shake);
    sprite->runAction(RepeatForever::create(shakeAn));
}

void MainLayer::moveCloud(cocos2d::ui::ImageView *cloud, float time1, float time2)
{
    float startX = cloud->getPosition().x;
    MoveTo *move1 = MoveTo::create(time1, Vec2(kWinSizeWidth + 130, cloud1->getPosition().y));
    MoveTo *move2 = MoveTo::create(0, Vec2(-kWinSizeWidth - 130, cloud1->getPosition().y));
    MoveTo *move3 = MoveTo::create(time2, Vec2(startX, cloud1->getPosition().y));
    Sequence *seque = Sequence::create(move1, move2, move3, NULL);
    cloud->runAction(RepeatForever::create(seque));
}

void MainLayer::showPlayerPanel()
{
    hidePlayerPanel();

    auto* pm = PlayerManager::getInstance();

    auto panel = Layout::create();
    panel->setContentSize(Size(kWinSizeWidth, kWinSizeHeight));
    panel->setBackGroundColorType(Layout::BackGroundColorType::SOLID);
    panel->setBackGroundColor(Color3B(0, 0, 0));
    panel->setBackGroundColorOpacity(200);
    panel->setAnchorPoint(Vec2::ANCHOR_MIDDLE);
    panel->setPosition(Vec2(kWinSizeWidth * 0.5f, kWinSizeHeight * 0.5f));
    panel->setName("PlayerPanel");
    this->addChild(panel, 50);

    auto inner = Layout::create();
    inner->setContentSize(Size(720, 360));
    inner->setAnchorPoint(Vec2::ANCHOR_MIDDLE);
    inner->setPosition(Vec2(kWinSizeWidth * 0.5f, kWinSizeHeight * 0.5f));
    inner->setBackGroundColorType(Layout::BackGroundColorType::SOLID);
    inner->setBackGroundColor(Color3B(50, 40, 30));
    inner->setBackGroundColorOpacity(255);
    inner->setName("InnerPanel");
    panel->addChild(inner);

    // Dismiss: touch outside inner closes panel
    auto dismissListener = EventListenerTouchOneByOne::create();
    dismissListener->setSwallowTouches(false);
    dismissListener->onTouchBegan = [this](Touch* touch, Event*) -> bool {
        if (!_playerPanel) return false;
        auto* inner = _playerPanel->getChildByName("InnerPanel");
        if (!inner) return false;
        Vec2 localPos = inner->convertToNodeSpace(touch->getLocation());
        Size sz = inner->getContentSize();
        Rect innerRect(-sz.width * 0.5f, -sz.height * 0.5f, sz.width, sz.height);
        if (!innerRect.containsPoint(localPos)) {
            hidePlayerPanel();
            return true;
        }
        return false; // let touch pass through to children
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(dismissListener, panel);
    _panelDismissListener = dismissListener;

    // Title
    auto title = Text::create("Player Manager — " + pm->currentPlayer(), "", 24);
    title->setPosition(Vec2(360, 330));
    title->setTextColor(Color4B::WHITE);
    inner->addChild(title);

    // Current player info
    auto curLabel = Text::create("Stage: " + to_string(pm->getStageNum()) + "  |  Money: $" + to_string(pm->getAllMoney()), "", 18);
    curLabel->setPosition(Vec2(360, 300));
    curLabel->setTextColor(Color4B(200, 200, 100, 255));
    curLabel->setName("curLabel");
    inner->addChild(curLabel);

    // Player list (full width, no history panel)
    auto playerListView = ListView::create();
    playerListView->setContentSize(Size(600, 190));
    playerListView->setPosition(Vec2(60, 100));
    playerListView->setDirection(ScrollView::Direction::VERTICAL);
    playerListView->setItemsMargin(4);
    playerListView->setName("playerList");
    inner->addChild(playerListView);

    auto playersLabel = Text::create("Players", "", 18);
    playersLabel->setPosition(Vec2(360, 275));
    playersLabel->setTextColor(Color4B(180, 180, 180, 255));
    inner->addChild(playersLabel);

    refreshPlayerList();

    // ── Close button ──
    auto closeBtn = makeCleanButton("Close", Size(200, 70), 22);
    closeBtn->setPosition(Vec2(360, 20));
    closeBtn->addTouchEventListener([this](Ref*, Widget::TouchEventType type) {
        if (type == Widget::TouchEventType::ENDED) hidePlayerPanel();
    });
    inner->addChild(closeBtn);

    _playerPanel = panel;
}

void MainLayer::hidePlayerPanel()
{
    if (_playerPanel) {
        if (_panelDismissListener) {
            _eventDispatcher->removeEventListener(_panelDismissListener);
            _panelDismissListener = nullptr;
        }
        _playerPanel->removeFromParent();
        _playerPanel = nullptr;
    }
    updatePlayerLabel();
}

void MainLayer::refreshPlayerList()
{
    if (!_playerPanel) return;
    auto* inner = _playerPanel->getChildByName("InnerPanel");
    if (!inner) return;
    auto* listView = static_cast<ListView*>(inner->getChildByName("playerList"));
    if (!listView) return;

    listView->removeAllItems();
    auto* pm = PlayerManager::getInstance();
    auto players = pm->listPlayers();

    for (const auto& name : players) {
        auto item = Layout::create();
        item->setContentSize(Size(580, 56));
        item->setBackGroundColorType(Layout::BackGroundColorType::SOLID);
        item->setBackGroundColor(Color3B(70, 60, 50));

        auto nameLabel = Text::create(name, "", 18);
        nameLabel->setPosition(Vec2(80, 28));
        nameLabel->setTextColor(Color4B::WHITE);
        item->addChild(nameLabel);

        auto selectBtn = makeCleanButton("Select", Size(140, 56), 18);
        selectBtn->setPosition(Vec2(330, 28));
        selectBtn->addTouchEventListener([this, pm, name](Ref*, Widget::TouchEventType type) {
            if (type != Widget::TouchEventType::ENDED) return;
            pm->selectPlayer(name);
            // Sync UserDataManager for explicit player selection
            _playerExplicitlySelected = true;
            auto* udm = UserDataManager::getInstance();
            udm->setStageNum(pm->getStageNum());
            udm->setAllMoney(pm->getAllMoney());
            if (_playerPanel) {
                auto* inner = _playerPanel->getChildByName("InnerPanel");
                if (inner) {
                    auto* curLabel = static_cast<Text*>(inner->getChildByName("curLabel"));
                    if (curLabel) curLabel->setString("Stage: " + to_string(pm->getStageNum()) + "  |  Money: $" + to_string(pm->getAllMoney()));
                }
            }
            this->refreshPlayerList();
            this->updatePlayerLabel();
        });
        item->addChild(selectBtn);

        auto deleteBtn = makeCleanButton("Del", Size(100, 56), 18);
        deleteBtn->setPosition(Vec2(480, 28));
        deleteBtn->addTouchEventListener([this, pm, name](Ref*, Widget::TouchEventType type) {
            if (type != Widget::TouchEventType::ENDED) return;
            pm->deletePlayer(name);
            if (_playerPanel) {
                auto* inner = _playerPanel->getChildByName("InnerPanel");
                if (inner) {
                    auto* curLabel = static_cast<Text*>(inner->getChildByName("curLabel"));
                    if (curLabel) curLabel->setString("Stage: " + to_string(pm->getStageNum()) + "  |  Money: $" + to_string(pm->getAllMoney()));
                }
            }
            this->refreshPlayerList();
            this->updatePlayerLabel();
        });
        item->addChild(deleteBtn);

        listView->pushBackCustomItem(item);
    }
}

void MainLayer::onModeBtnTouch(int mode)
{
    if (_selectedMode == mode) return;
    _selectedMode = mode;

    auto highlight = [](Button* btn, bool active) {
        if (btn) btn->setBright(active);
    };
    highlight(_touchModeBtn, mode == 0);
    highlight(_opencvModeBtn, mode == 1);
    highlight(_aiModeBtn, mode == 2);
}

void MainLayer::updatePlayerLabel()
{
    auto* pm = PlayerManager::getInstance();
    auto players = pm->listPlayers();
    if (players.empty()) {
        pm->createPlayer("Player1");
        players = pm->listPlayers();
    }
    _playerSelected = !players.empty();

    if (_playerBtn) {
        _playerBtn->setTitleText(_playerSelected
            ? "Player: " + pm->currentPlayer() : "Player: --");
    }
    if (_startBtn) {
        _startBtn->setEnabled(true);
        _startBtn->setBright(true);
    }
}