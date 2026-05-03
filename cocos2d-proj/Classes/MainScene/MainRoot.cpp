//
//  MainRoot.cpp
//  GoldMiner
//
//  Created by MacBook on 16/11/22.
//
//

#include "MainRoot.hpp"
#include "Game.hpp"
#include "SimpleAudioEngine.h"
#include "UserDataManager.hpp"
#include "Shop.hpp"
#include "SoundTool.hpp"
#include "Const.hpp"
#include "services/PlayerManager.hpp"

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
    _playerBtn->setContentSize(Size(160, 50));
    _playerBtn->addTouchEventListener([this](Ref*, Widget::TouchEventType type) {
        if (type == Widget::TouchEventType::ENDED) {
            kPlayClickSound
            showPlayerPanel();
        }
    });
    addChild(_playerBtn, 10);

    // Mode selection buttons at bottom-left
    auto makeModeBtn = [&](const std::string& text, int mode, float x, float y) -> Button* {
        auto btn = Button::create("Resources/button.png");
        btn->setPosition(Vec2(x, y));
        btn->setTitleText(text);
        btn->setTitleFontSize(20);
        btn->setTitleColor(Color3B::BLACK);
        btn->setScale9Enabled(true);
        btn->setContentSize(Size(100, 40));
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

    _opencvModeBtn = makeModeBtn("OPENCV", 1, mx + 120, my);
    addChild(_opencvModeBtn, 10);

    _aiModeBtn = makeModeBtn("AI", 2, mx + 240, my);
    addChild(_aiModeBtn, 10);

    // Highlight default mode
    onModeBtnTouch(0);

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
    
    SoundTool::getInstance()->playBackgroundMusic("music/backMusic.mp3");
}

void MainLayer::startButtonTouch(cocos2d::Ref *sender, Widget::TouchEventType type)
{
    if (type == Widget::TouchEventType::ENDED) {
        if (!_playerSelected) return;

        kPlayClickSound

        // Set the input mode for the game
        InputMode mode = InputMode::TOUCH;
        if (_selectedMode == 1) mode = InputMode::OPENCV;
        else if (_selectedMode == 2) mode = InputMode::AI;
        Game::setDefaultInputMode(mode);

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
    panel->addTouchEventListener([=](Ref*, Widget::TouchEventType type) {
        if (type == Widget::TouchEventType::ENDED) hidePlayerPanel();
    });
    this->addChild(panel, 50);

    auto inner = Layout::create();
    inner->setContentSize(Size(720, 500));
    inner->setAnchorPoint(Vec2::ANCHOR_MIDDLE);
    inner->setPosition(Vec2(kWinSizeWidth * 0.5f, kWinSizeHeight * 0.5f));
    inner->setBackGroundColorType(Layout::BackGroundColorType::SOLID);
    inner->setBackGroundColor(Color3B(50, 40, 30));
    inner->setBackGroundColorOpacity(255);
    inner->setTouchEnabled(true);
    inner->setSwallowTouches(true);
    inner->setName("InnerPanel");
    panel->addChild(inner);

    // Title
    auto title = Text::create("Player Manager — " + pm->currentPlayer(), "", 24);
    title->setPosition(Vec2(360, 470));
    title->setTextColor(Color4B::WHITE);
    inner->addChild(title);

    // Current player info
    auto curLabel = Text::create("Stage: " + to_string(pm->getStageNum()) + "  |  Money: $" + to_string(pm->getAllMoney()), "", 18);
    curLabel->setPosition(Vec2(360, 440));
    curLabel->setTextColor(Color4B(200, 200, 100, 255));
    curLabel->setName("curLabel");
    inner->addChild(curLabel);

    // ── Left: Player List ──
    auto playerListView = ListView::create();
    playerListView->setContentSize(Size(340, 220));
    playerListView->setPosition(Vec2(20, 200));
    playerListView->setDirection(ScrollView::Direction::VERTICAL);
    playerListView->setItemsMargin(4);
    playerListView->setName("playerList");
    inner->addChild(playerListView);

    auto playersLabel = Text::create("Players", "", 18);
    playersLabel->setPosition(Vec2(190, 430));
    playersLabel->setTextColor(Color4B(180, 180, 180, 255));
    inner->addChild(playersLabel);

    refreshPlayerList();

    // ── Right: History ──
    auto historyLabel = Text::create("History", "", 18);
    historyLabel->setPosition(Vec2(530, 430));
    historyLabel->setTextColor(Color4B(180, 180, 180, 255));
    inner->addChild(historyLabel);

    auto historyList = ListView::create();
    historyList->setContentSize(Size(350, 220));
    historyList->setPosition(Vec2(370, 200));
    historyList->setDirection(ScrollView::Direction::VERTICAL);
    historyList->setItemsMargin(3);
    historyList->setName("historyList");
    inner->addChild(historyList);

    auto refreshHistory = [this, pm]() {
        if (!_playerPanel) return;
        auto* inner = _playerPanel->getChildByName("InnerPanel");
        if (!inner) return;
        auto* listView = static_cast<ListView*>(inner->getChildByName("historyList"));
        if (!listView) return;
        listView->removeAllItems();

        auto history = pm->getHistory();
        if (history.empty()) {
            auto emptyLabel = Text::create("No history yet", "", 16);
            emptyLabel->setTextColor(Color4B(150, 150, 150, 255));
            auto item = Layout::create();
            item->setContentSize(Size(330, 30));
            emptyLabel->setPosition(Vec2(165, 15));
            item->addChild(emptyLabel);
            listView->pushBackCustomItem(item);
        } else {
            for (auto it = history.rbegin(); it != history.rend(); ++it) {
                auto item = Layout::create();
                item->setContentSize(Size(330, 28));
                item->setBackGroundColorType(Layout::BackGroundColorType::SOLID);
                item->setBackGroundColor(Color3B(70, 60, 50));

                std::string line = "Stage " + to_string(it->stageNum)
                    + "  Score:" + to_string(it->score)
                    + "  " + std::string(it->passed ? "PASS" : "FAIL");
                auto lbl = Text::create(line, "", 13);
                lbl->setPosition(Vec2(165, 14));
                lbl->setTextColor(it->passed ? Color4B(100, 200, 100, 255) : Color4B(220, 100, 100, 255));
                item->addChild(lbl);

                listView->pushBackCustomItem(item);
            }
        }
    };
    refreshHistory();

    // ── Bottom: Create new player ──
    auto inputBg = Layout::create();
    inputBg->setContentSize(Size(240, 36));
    inputBg->setBackGroundColorType(Layout::BackGroundColorType::SOLID);
    inputBg->setBackGroundColor(Color3B(80, 70, 60));
    inputBg->setPosition(Vec2(20, 140));
    inputBg->setName("inputBg");
    inner->addChild(inputBg);

    auto inputField = TextField::create("Enter name...", "", 16);
    inputField->setPosition(Vec2(120, 18));
    inputField->setMaxLength(12);
    inputField->setMaxLengthEnabled(true);
    inputField->setName("inputField");
    inputField->setTextColor(Color4B::WHITE);
    inputField->setCursorChar('|');
    inputBg->addChild(inputField);

    auto createBtn = Button::create("Resources/button.png");
    createBtn->setTitleText("Create");
    createBtn->setTitleFontSize(18);
    createBtn->setTitleColor(Color3B::BLACK);
    createBtn->setScale9Enabled(true);
    createBtn->setContentSize(Size(100, 36));
    createBtn->setPosition(Vec2(200, 140));
    createBtn->addTouchEventListener([this, pm, refreshHistory](Ref*, Widget::TouchEventType type) {
        if (type != Widget::TouchEventType::ENDED) return;
        auto* node = this->getChildByName("PlayerPanel");
        if (!node) return;
        auto* inner = node->getChildByName("InnerPanel");
        if (!inner) return;
        auto* inputBg = inner->getChildByName("inputBg");
        auto* tf = static_cast<TextField*>(inputBg->getChildByName("inputField"));
        if (!tf) return;
        std::string name = tf->getString();
        if (name.empty()) return;
        pm->createPlayer(name);
        tf->setString("");
        auto* curLabel = static_cast<Text*>(inner->getChildByName("curLabel"));
        if (curLabel) curLabel->setString("Stage: " + to_string(pm->getStageNum()) + "  |  Money: $" + to_string(pm->getAllMoney()));
        this->refreshPlayerList();
        this->updatePlayerLabel();
    });
    inner->addChild(createBtn);

    // Close button
    auto closeBtn = Button::create("Resources/button.png");
    closeBtn->setTitleText("Close");
    closeBtn->setTitleFontSize(18);
    closeBtn->setTitleColor(Color3B::BLACK);
    closeBtn->setScale9Enabled(true);
    closeBtn->setContentSize(Size(100, 35));
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
        item->setContentSize(Size(320, 36));
        item->setBackGroundColorType(Layout::BackGroundColorType::SOLID);
        item->setBackGroundColor(Color3B(70, 60, 50));

        auto nameLabel = Text::create(name, "", 18);
        nameLabel->setPosition(Vec2(80, 18));
        nameLabel->setTextColor(Color4B::WHITE);
        item->addChild(nameLabel);

        auto selectBtn = Button::create("Resources/button.png");
        selectBtn->setTitleText("Select");
        selectBtn->setTitleFontSize(15);
        selectBtn->setTitleColor(Color3B::BLACK);
        selectBtn->setScale9Enabled(true);
        selectBtn->setContentSize(Size(70, 28));
        selectBtn->setPosition(Vec2(190, 18));
        selectBtn->addTouchEventListener([this, pm, name](Ref*, Widget::TouchEventType type) {
            if (type != Widget::TouchEventType::ENDED) return;
            pm->selectPlayer(name);
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

        auto deleteBtn = Button::create("Resources/button.png");
        deleteBtn->setTitleText("Del");
        deleteBtn->setTitleFontSize(15);
        deleteBtn->setTitleColor(Color3B::BLACK);
        deleteBtn->setScale9Enabled(true);
        deleteBtn->setContentSize(Size(50, 28));
        deleteBtn->setPosition(Vec2(280, 18));
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
    _selectedMode = mode;

    // Switch background music based on mode
    if (mode == 0) {
        SoundTool::getInstance()->playBackgroundMusic("music/backMusic.mp3");
    } else {
        SoundTool::getInstance()->playBackgroundMusic("music/backMusic-exchange.flac");
        CocosDenshion::SimpleAudioEngine::getInstance()->setBackgroundMusicVolume(0.5f);
    }

    auto highlight = [](Button* btn, bool active) {
        if (btn) btn->setColor(active ? Color3B(100, 255, 100) : Color3B::WHITE);
    };
    highlight(_touchModeBtn, mode == 0);
    highlight(_opencvModeBtn, mode == 1);
    highlight(_aiModeBtn, mode == 2);
}

void MainLayer::updatePlayerLabel()
{
    auto* pm = PlayerManager::getInstance();
    auto players = pm->listPlayers();
    _playerSelected = !players.empty();

    if (_playerBtn) {
        _playerBtn->setTitleText(_playerSelected
            ? "Player: " + pm->currentPlayer() : "Player: --");
    }
    if (_startBtn) {
        _startBtn->setEnabled(_playerSelected);
        _startBtn->setBright(_playerSelected);
    }
}