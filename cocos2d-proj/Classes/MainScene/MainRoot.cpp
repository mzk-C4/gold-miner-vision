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
    auto startBtn = static_cast<Button *>(Helper::seekWidgetByName(static_cast<Widget *>(mainCsb), "startButton"));
    startBtn->addTouchEventListener(CC_CALLBACK_2(MainLayer::startButtonTouch, this));

    // Player and History buttons — same style as startButton
    auto playerBtn = Button::create("Resources/menu-start-gold.png");
    playerBtn->setPosition(Vec2(kWinSizeWidth * 0.85f, kWinSizeHeight * 0.65f));
    playerBtn->setTitleText("P");
    playerBtn->setTitleFontSize(24);
    playerBtn->setTitleColor(Color3B::WHITE);
    playerBtn->addTouchEventListener(CC_CALLBACK_2(MainLayer::playerButtonTouch, this));
    addChild(playerBtn, 10);

    auto historyBtn = Button::create("Resources/menu-start-gold.png");
    historyBtn->setPosition(Vec2(kWinSizeWidth * 0.85f, kWinSizeHeight * 0.45f));
    historyBtn->setTitleText("H");
    historyBtn->setTitleFontSize(24);
    historyBtn->setTitleColor(Color3B::WHITE);
    historyBtn->addTouchEventListener(CC_CALLBACK_2(MainLayer::historyButtonTouch, this));
    addChild(historyBtn, 10);

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
        
        kPlayClickSound
        
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

void MainLayer::playerButtonTouch(Ref *sender, Widget::TouchEventType type)
{
    if (type == Widget::TouchEventType::ENDED) {
        kPlayClickSound
        showPlayerPanel();
    }
}

void MainLayer::historyButtonTouch(Ref *sender, Widget::TouchEventType type)
{
    if (type == Widget::TouchEventType::ENDED) {
        kPlayClickSound
        showHistoryPanel();
    }
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
    inner->setContentSize(Size(500, 520));
    inner->setAnchorPoint(Vec2::ANCHOR_MIDDLE);
    inner->setPosition(Vec2(kWinSizeWidth * 0.5f, kWinSizeHeight * 0.5f));
    inner->setBackGroundColorType(Layout::BackGroundColorType::SOLID);
    inner->setBackGroundColor(Color3B(50, 40, 30));
    inner->setBackGroundColorOpacity(255);
    inner->setTouchEnabled(true);
    inner->addTouchEventListener([=](Ref*, Widget::TouchEventType) {});
    inner->setName("InnerPanel");
    panel->addChild(inner);

    auto title = Text::create("Player Manager", "", 28);
    title->setPosition(Vec2(250, 490));
    title->setTextColor(Color4B::WHITE);
    inner->addChild(title);

    auto curLabel = Text::create("Current: " + pm->currentPlayer(), "", 20);
    curLabel->setPosition(Vec2(250, 450));
    curLabel->setTextColor(Color4B(200, 200, 100, 255));
    curLabel->setName("curLabel");
    inner->addChild(curLabel);

    // Player list
    auto listView = ListView::create();
    listView->setContentSize(Size(460, 280));
    listView->setPosition(Vec2(20, 150));
    listView->setDirection(ScrollView::Direction::VERTICAL);
    listView->setItemsMargin(4);
    listView->setName("playerList");
    inner->addChild(listView);

    refreshPlayerList();

    // New player input
    auto inputBg = Layout::create();
    inputBg->setContentSize(Size(300, 40));
    inputBg->setBackGroundColorType(Layout::BackGroundColorType::SOLID);
    inputBg->setBackGroundColor(Color3B(80, 70, 60));
    inputBg->setPosition(Vec2(20, 90));
    inputBg->setName("inputBg");
    inner->addChild(inputBg);

    auto inputField = TextField::create("Enter name...", "", 18);
    inputField->setPosition(Vec2(150, 20));
    inputField->setMaxLength(12);
    inputField->setMaxLengthEnabled(true);
    inputField->setName("inputField");
    inputField->setTextColor(Color4B::WHITE);
    inputField->setCursorChar('|');
    inputBg->addChild(inputField);

    auto createBtn = Button::create("Default/Button_Disable.png");
    createBtn->setTitleText("Create");
    createBtn->setTitleFontSize(16);
    createBtn->setContentSize(Size(140, 40));
    createBtn->setPosition(Vec2(350, 90));
    createBtn->addTouchEventListener([this, pm](Ref*, Widget::TouchEventType type) {
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
        // Update current label
        auto* curLabel = static_cast<Text*>(inner->getChildByName("curLabel"));
        if (curLabel) curLabel->setString("Current: " + pm->currentPlayer());
        refreshPlayerList();
    });
    inner->addChild(createBtn);

    // Close button
    auto closeBtn = Button::create("Default/Button_Disable.png");
    closeBtn->setTitleText("Close");
    closeBtn->setTitleFontSize(16);
    closeBtn->setContentSize(Size(100, 35));
    closeBtn->setPosition(Vec2(250, 30));
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
        item->setContentSize(Size(440, 40));
        item->setBackGroundColorType(Layout::BackGroundColorType::SOLID);
        item->setBackGroundColor(Color3B(70, 60, 50));

        auto nameLabel = Text::create(name, "", 20);
        nameLabel->setPosition(Vec2(100, 20));
        nameLabel->setTextColor(Color4B::WHITE);
        item->addChild(nameLabel);

        auto selectBtn = Button::create("Default/Button_Disable.png");
        selectBtn->setTitleText("Select");
        selectBtn->setTitleFontSize(14);
        selectBtn->setContentSize(Size(80, 32));
        selectBtn->setPosition(Vec2(250, 20));
        selectBtn->addTouchEventListener([this, pm, name](Ref*, Widget::TouchEventType type) {
            if (type != Widget::TouchEventType::ENDED) return;
            pm->selectPlayer(name);
            refreshPlayerList();
            if (_playerPanel) {
                auto* inner = _playerPanel->getChildByName("InnerPanel");
                if (inner) {
                    auto* curLabel = static_cast<Text*>(inner->getChildByName("curLabel"));
                    if (curLabel) curLabel->setString("Current: " + pm->currentPlayer());
                }
            }
        });
        item->addChild(selectBtn);

        auto deleteBtn = Button::create("Default/Button_Disable.png");
        deleteBtn->setTitleText("Del");
        deleteBtn->setTitleFontSize(14);
        deleteBtn->setContentSize(Size(60, 32));
        deleteBtn->setPosition(Vec2(350, 20));
        deleteBtn->addTouchEventListener([this, pm, name](Ref*, Widget::TouchEventType type) {
            if (type != Widget::TouchEventType::ENDED) return;
            pm->deletePlayer(name);
            refreshPlayerList();
            if (_playerPanel) {
                auto* inner = _playerPanel->getChildByName("InnerPanel");
                if (inner) {
                    auto* curLabel = static_cast<Text*>(inner->getChildByName("curLabel"));
                    if (curLabel) curLabel->setString("Current: " + pm->currentPlayer());
                }
            }
        });
        item->addChild(deleteBtn);

        listView->pushBackCustomItem(item);
    }
}

void MainLayer::showHistoryPanel()
{
    hideHistoryPanel();

    auto* pm = PlayerManager::getInstance();

    auto panel = Layout::create();
    panel->setContentSize(Size(kWinSizeWidth, kWinSizeHeight));
    panel->setBackGroundColorType(Layout::BackGroundColorType::SOLID);
    panel->setBackGroundColor(Color3B(0, 0, 0));
    panel->setBackGroundColorOpacity(200);
    panel->setAnchorPoint(Vec2::ANCHOR_MIDDLE);
    panel->setPosition(Vec2(kWinSizeWidth * 0.5f, kWinSizeHeight * 0.5f));
    panel->setName("HistoryPanel");
    panel->addTouchEventListener([=](Ref*, Widget::TouchEventType type) {
        if (type == Widget::TouchEventType::ENDED) hideHistoryPanel();
    });
    this->addChild(panel, 50);

    auto inner = Layout::create();
    inner->setContentSize(Size(550, 500));
    inner->setAnchorPoint(Vec2::ANCHOR_MIDDLE);
    inner->setPosition(Vec2(kWinSizeWidth * 0.5f, kWinSizeHeight * 0.5f));
    inner->setBackGroundColorType(Layout::BackGroundColorType::SOLID);
    inner->setBackGroundColor(Color3B(50, 40, 30));
    inner->setBackGroundColorOpacity(255);
    inner->setTouchEnabled(true);
    inner->addTouchEventListener([=](Ref*, Widget::TouchEventType) {});
    inner->setName("InnerPanel");
    panel->addChild(inner);

    auto title = Text::create("History — " + pm->currentPlayer(), "", 24);
    title->setPosition(Vec2(275, 470));
    title->setTextColor(Color4B::WHITE);
    inner->addChild(title);

    auto listView = ListView::create();
    listView->setContentSize(Size(510, 400));
    listView->setPosition(Vec2(20, 50));
    listView->setDirection(ScrollView::Direction::VERTICAL);
    listView->setItemsMargin(4);
    inner->addChild(listView);

    auto history = pm->getHistory();
    if (history.empty()) {
        auto emptyLabel = Text::create("No game history yet", "", 20);
        emptyLabel->setTextColor(Color4B(150, 150, 150, 255));
        auto item = Layout::create();
        item->setContentSize(Size(490, 50));
        emptyLabel->setPosition(Vec2(245, 25));
        item->addChild(emptyLabel);
        listView->pushBackCustomItem(item);
    } else {
        for (auto it = history.rbegin(); it != history.rend(); ++it) {
            auto item = Layout::create();
            item->setContentSize(Size(490, 50));
            item->setBackGroundColorType(Layout::BackGroundColorType::SOLID);
            item->setBackGroundColor(Color3B(70, 60, 50));

            std::string line = "Stage " + to_string(it->stageNum)
                + " | Score: " + to_string(it->score)
                + " | Target: " + to_string(it->targetMoney)
                + " | " + std::string(it->passed ? "PASS" : "FAIL")
                + " | " + it->timestamp;

            auto label = Text::create(line, "", 16);
            label->setPosition(Vec2(245, 25));
            label->setTextColor(it->passed ? Color4B(100, 200, 100, 255) : Color4B(220, 100, 100, 255));
            item->addChild(label);

            listView->pushBackCustomItem(item);
        }
    }

    auto closeBtn = Button::create("Default/Button_Disable.png");
    closeBtn->setTitleText("Close");
    closeBtn->setTitleFontSize(16);
    closeBtn->setContentSize(Size(100, 35));
    closeBtn->setPosition(Vec2(275, 20));
    closeBtn->addTouchEventListener([this](Ref*, Widget::TouchEventType type) {
        if (type == Widget::TouchEventType::ENDED) hideHistoryPanel();
    });
    inner->addChild(closeBtn);

    _historyPanel = panel;
}

void MainLayer::hideHistoryPanel()
{
    if (_historyPanel) {
        _historyPanel->removeFromParent();
        _historyPanel = nullptr;
    }
}