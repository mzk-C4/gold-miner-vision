//
//  Game.cpp
//  GoldMiner
//
//  Created by MacBook on 16/11/27.
//
//

#include "Game.hpp"
#include "Pause.hpp"
#include "UserDataManager.hpp"
#include "StageTipsLayer.hpp"
#include "Shop.hpp"
#include "GestureClient.hpp"
#include "AIGestureService.hpp"
#include "platform/CCImage.h"
#include "SimpleAudioEngine.h"
#include <vector>
#include <cstdlib>

#define kWorldTag 1000

InputMode Game::_defaultInputMode = InputMode::TOUCH;

Scene *Game::createScene(bool isBuyBomb, bool isBuyPotion, bool isBuyDiamonds, bool isStoneBook, int payMoney)
{
    Scene *scene = Scene::createWithPhysics();
    
    auto world = scene->getPhysicsWorld();
//    world->setDebugDrawMask(PhysicsWorld::DEBUGDRAW_ALL);
    world->setGravity(Vec2::ZERO);
    
    auto gameLayer = Game::create(isBuyBomb, isBuyPotion, isBuyDiamonds, isStoneBook, payMoney);
    scene->addChild(gameLayer);
    
    PhysicsBody *body = PhysicsBody::createEdgeBox(Size(kWinSizeWidth, kWinSizeHeight));
    body->setCategoryBitmask(10);
    body->setCollisionBitmask(10);
    body->setContactTestBitmask(10);
    
    Node *node = Node::create();
    node->setPosition(kWinSize * 0.5);
    node->addComponent(body);
    node->setColor(Color3B::RED);
    node->setTag(kWorldTag);
    scene->addChild(node);
    
    SpriteFrameCache::getInstance()->addSpriteFramesWithFile("Resources/level-sheet.plist");
    
    return scene;
}

Game *Game::create(bool isBuyBomb, bool isBuyPotion, bool isBuyDiamonds, bool isStoneBook, int payMoney)
{
    Game *pRet = new Game();
    if (pRet && pRet->init(isBuyBomb, isBuyPotion, isBuyDiamonds, isStoneBook, payMoney))
    {
        pRet->autorelease();
        return pRet;
    }
    else
    {
        delete pRet;
        pRet = nullptr;
        return nullptr;
    }
}

bool Game::init(bool isBuyBomb, bool isBuyPotion, bool isBuyDiamonds, bool isStoneBook, int payMoney)
{
    if (!Layer::init()) return false;

    auto csb = CSLoader::createNode("GameLayer.csb");
    if (!csb) {
        CCLOG("ERROR: Failed to load GameLayer.csb");
        return false;
    }
    this->addChild(csb, 10);
    
    this->isBuyPotion = isBuyPotion;
    this->isBuyDiamonds = isBuyDiamonds;
    this->isBuyStoneBook = isStoneBook;
    
    bompButton = static_cast<Button *>(Helper::seekWidgetByName(static_cast<Widget *>(csb), "BompButton"));
    bompButton->setVisible(isBuyBomb);
    bompButton->addTouchEventListener([=](Ref *ref, Widget::TouchEventType type){
        if (type == Widget::TouchEventType::ENDED) {
            // click bomp
            if (isOpenHook) {
                bompButton->setVisible(false);
                // 炸毁物品
                backSpeed = 10;
                
                isOpenHook = false;
                leftHook->setRotation(0);
                rightHook->setRotation(0);
                
                //爆炸效果
                auto postion = rope->convertToWorldSpace(middleCircle->getPosition());
                ParticleSystemQuad *bompEm = ParticleSystemQuad::create("Boom.plist");
                bompEm->setPosition(postion);
                this->addChild(bompEm);
                
                SoundTool::getInstance()->playEffect("music/bomb.mp3");
                
                _hookedMineral->removeFromParent();
                
            }
        }
    });
    
    // 获取序列帧动画
    minerTimeLine = CSLoader::createTimeline("GameLayer.csb");
    this->runAction(minerTimeLine);
    
    auto hookCsb = CSLoader::createNode("Hook.csb");
    if (!hookCsb) {
        CCLOG("ERROR: Failed to load Hook.csb");
        return false;
    }
    hookCsb->setPosition(kWinSizeWidth * 0.48, kWinSizeHeight * 0.856);
    this->addChild(hookCsb, 11);
    
    rope = static_cast<ImageView *>(Helper::seekWidgetByName(static_cast<Widget *>(hookCsb), "rope"));
    middleCircle = static_cast<Sprite *>(rope->getChildByTag(59));
    leftHook = static_cast<Sprite *>(middleCircle->getChildByTag(60));
    rightHook = static_cast<Sprite *>(middleCircle->getChildByTag(61));
    curPayMoney = payMoney;
    
    // 添加钩子刚体
    PhysicsBody *hookBody = PhysicsBody::createCircle(20);
    hookBody->setContactTestBitmask(10);
    hookBody->setCollisionBitmask(10);
    hookBody->setCategoryBitmask(10);
    middleCircle->addComponent(hookBody);
    circlePosition = middleCircle->getPosition();
    this->addButtonAction(csb);
    
    setUpText(static_cast<Widget *>(csb));
    
    timeCount = 60;
    
    // 添加碰撞事件
    auto physicsListener = EventListenerPhysicsContact::create();
    physicsListener->onContactBegin = CC_CALLBACK_1(Game::physicsBegin, this);
    _eventDispatcher->addEventListenerWithSceneGraphPriority(physicsListener, this);
    _eventDispatcher->removeCustomEventListeners("nextStage");
    _eventDispatcher->addCustomEventListener("nextStage", [=](EventCustom *cus){
        // 过关成功,保存数据
        UserDataManager *user = UserDataManager::getInstance();
        user->setAllMoney(UserDataManager::getInstance()->getAllMoney() - curPayMoney + curStageScore);
        user->setStageNum(user->getStageNum()+1);
        user->saveUserData();

        // 进入商店
        Scene *shopScene = Shop::createScene();
        Director::getInstance()->replaceScene(shopScene);
    });

    _eventDispatcher->addCustomEventListener("resumeGame", [=](EventCustom *cus){
        if (_isPaused) {
            _isPaused = false;
            this->startGame();
        }
    });

    loadStageInfo();
    setupModePanel(csb);

    // Apply default mode from home screen
    if (_defaultInputMode != InputMode::TOUCH) {
        switchInputMode(_defaultInputMode);
    }

    return true;
}

bool Game::physicsBegin(cocos2d::PhysicsContact &contact)
{
    if (contact.getShapeB()->getBody()->getNode()->getTag() != kWorldTag) {
        // 碰到金块, 打开钩子
        if (!isOpenHook) {
            this->pullGold(contact);
        }
    } else {
        this->backSpeed = 10;
    }
    
    this->unschedule(CC_SCHEDULE_SELECTOR(Game::addRopeHeight));
    this->schedule(CC_SCHEDULE_SELECTOR(Game::subRopeHeight), 0.025);
    
    return true;
}

void Game::loadStageInfo()
{
    // 加载关卡信息
    int stageNum = UserDataManager::getInstance()->getStageNum();
    int levelIndex = stageNum % 5;
    if (levelIndex == 0) {
        levelIndex = 5;
    }
    
    string levelCsbName = "level" + to_string(levelIndex) + ".csb";
    auto goldCsb = CSLoader::createNode(levelCsbName);
    if (!goldCsb) {
        CCLOG("ERROR: Failed to load %s", levelCsbName.c_str());
        return;
    }
    this->addChild(goldCsb);
    
    // 所有金块
    auto goldsLayout = Helper::seekWidgetByName(static_cast<Widget *>(goldCsb), "goldPanel");
    Vector<Node *> golds = goldsLayout->getChildren();
    for (Node *subNode : golds) {
        Size bodySize = Size(subNode->getContentSize().width * subNode->getScaleX(), subNode->getContentSize().height * subNode->getScaleY());
        PhysicsBody *goldBody = PhysicsBody::createEdgeBox(bodySize);
        goldBody->setCategoryBitmask(10);
        goldBody->setCollisionBitmask(10);
        goldBody->setContactTestBitmask(10);
        subNode->addComponent(goldBody);
    }
}

void Game::pullGold(cocos2d::PhysicsContact &contact)
{
    // 钓到了东西
    isOpenHook = true;
    
    auto gold = contact.getShapeB()->getBody()->getNode();
    
    _hookedMineral = Mineral::create(gold->getName(), gold->getScaleX(), gold->getScaleY(), gold->getRotation(), isBuyPotion, isBuyDiamonds, isBuyStoneBook);
    middleCircle->addChild(_hookedMineral);
    contact.getShapeB()->getBody()->removeFromWorld();
    this->backSpeed = _hookedMineral->backSpeed;
    leftHook->runAction(RotateTo::create(0.05, -_hookedMineral->hookRote));
    rightHook->runAction(RotateTo::create(0.05, _hookedMineral->hookRote));
    
    gold->getPhysicsBody()->setEnabled(false);
    gold->setVisible(false);
}

void Game::subRopeHeight(float dt)
{
    middleCircle->setPosition(circlePosition);
    
    ropeHeight -= backSpeed;
    if (ropeHeight <= 20) {
        ropeHeight = 20;
        minerTimeLine->pause();
        // 恢复原样, 继续摇摆
        this->startShakeHookAnimation();
        this->unschedule(CC_SCHEDULE_SELECTOR(Game::subRopeHeight));
        ropeChangeing = false;
        
        if (isOpenHook) {
            isOpenHook = false;
            leftHook->setRotation(0);
            rightHook->setRotation(0);
            
            if (_hookedMineral != nullptr) {
                // 加分动画
                Label *scoreLabel = Label::create();
                scoreLabel->setColor(Color3B(50, 200, 0));
                scoreLabel->setSystemFontSize(25);
                scoreLabel->setString(to_string(_hookedMineral->score));
                scoreLabel->setPosition(rope->convertToWorldSpace(middleCircle->getPosition()));
                this->addChild(scoreLabel, 1000);
                
                SoundTool::getInstance()->playEffect("music/laend.mp3");
                
                curStageScore += _hookedMineral->score;
                auto spawn = Spawn::create(MoveTo::create(0.5, Vec2(allMoney->getPosition().x + 10, allMoney->getPosition().y)), Sequence::create(ScaleTo::create(0.25, 3), ScaleTo::create(0.25, 0.1), NULL), NULL);
                auto seque = Sequence::create(spawn, CallFuncN::create([=](Node *node){
                    
                    scoreLabel->removeFromParent();
                    allMoney->setString(to_string(curStageScore + UserDataManager::getInstance()->getAllMoney() - curPayMoney));
                    
                }),NULL);
                scoreLabel->runAction(seque);
                
                // 加分
                _hookedMineral->removeFromParent();
                _hookedMineral = nullptr;
            }
        }
    }
    
    rope->setContentSize(Size(3, ropeHeight));
}

void Game::setUpText(Widget *csb)
{
    auto userManager = UserDataManager::getInstance();
    
    allMoney = static_cast<Text *>(Helper::seekWidgetByName(csb, "allMoney"));
    targetMoney = static_cast<Text *>(Helper::seekWidgetByName(csb, "passScore"));
    stageNum = static_cast<Text *>(Helper::seekWidgetByName(csb, "stage"));
    time = static_cast<Text *>(Helper::seekWidgetByName(csb, "time"));
    
    int stageIndex = userManager->getStageNum();
    allMoney->setString(to_string(userManager->getAllMoney() - curPayMoney));
    stageNum->setString(to_string(stageIndex));
    time->setString("60");
    passScroe = 650 + 275 * (stageIndex - 1) + 410 * (stageIndex - 1);
    targetMoney->setString(to_string(passScroe));
}

void Game::onEnter()
{
    Layer::onEnter();

    if (!showStageTips) {
        showStageTips = true;

        SoundTool::getInstance()->playEffect("music/level.mp3");

        StageTipsLayer::showStageTipsLayer(this, UserDataManager::getInstance()->getStageNum(), [=](){
            this->startGame();
        });
    } else if (_isPaused) {
        _isPaused = false;
        this->startGame();
    }
}

void Game::updateTime(float dt)
{
    timeCount--;
    time->setString(to_string(timeCount));
    
    if (timeCount == 0) {
        this->stopGame();
    }
}

void Game::startGame()
{
    // Gesture mode: play random game music at lower volume
    if (_inputMode != InputMode::TOUCH) {
        static const std::vector<std::string> gameSongs = {
            "music/game/David Tao/陶喆 - Angel.mp3",
            "music/game/David Tao/陶喆 - Melody.mp3",
            "music/game/David Tao/陶喆 - 二十二.mp3",
            "music/game/David Tao/陶喆 - 寂寞的季节.mp3",
            "music/game/David Tao/陶喆 - 小镇姑娘.mp3",
            "music/game/David Tao/陶喆 - 就是爱你.mp3",
            "music/game/David Tao/陶喆 - 找自己.mp3",
            "music/game/David Tao/陶喆 - 普通朋友.mp3",
            "music/game/David Tao/陶喆 - 暗恋.mp3",
            "music/game/David Tao/陶喆 - 望春风.mp3",
            "music/game/David Tao/陶喆 - 流沙.mp3",
            "music/game/David Tao/陶喆 - 爱我还是他.mp3",
            "music/game/David Tao/陶喆 - 爱，很简单.mp3",
            "music/game/David Tao/陶喆 - 蝴蝶.mp3",
            "music/game/David Tao/陶喆 - 讨厌红楼梦.mp3",
            "music/game/David Tao/陶喆 - 飞机场的1030.mp3",
            "music/game/JAY/周杰伦 - 三年二班.flac",
            "music/game/JAY/周杰伦 - 上海一九四三.flac",
            "music/game/JAY/周杰伦 - 东风破.flac",
            "music/game/JAY/周杰伦 - 你听得到.flac",
            "music/game/JAY/周杰伦 - 分裂.flac",
            "music/game/JAY/周杰伦 - 双刀.flac",
            "music/game/JAY/周杰伦 - 反方向的钟.flac",
            "music/game/JAY/周杰伦 - 外婆.flac",
            "music/game/JAY/周杰伦 - 大笨钟.flac",
            "music/game/JAY/周杰伦 - 将军.flac",
            "music/game/JAY/周杰伦 - 开不了口.flac",
            "music/game/JAY/周杰伦 - 忍者.flac",
            "music/game/JAY/周杰伦 - 手语.flac",
            "music/game/JAY/周杰伦 - 斗牛.flac",
            "music/game/JAY/周杰伦 - 时光机.flac",
            "music/game/JAY/周杰伦 - 晴天.flac",
            "music/game/JAY/周杰伦 - 火车叨位去.flac",
            "music/game/JAY/周杰伦 - 爱你没差.flac",
            "music/game/JAY/周杰伦 - 爱在西元前.flac",
            "music/game/JAY/周杰伦 - 牛仔很忙.flac",
            "music/game/JAY/周杰伦 - 稻香.flac",
            "music/game/JAY/周杰伦 - 给我一首歌的时间.flac",
            "music/game/JAY/周杰伦 - 说走就走.flac",
            "music/game/JAY/周杰伦 - 逆鳞.flac",
            "music/game/JAY/周杰伦 - 霍元甲.flac",
            "music/game/JAY/周杰伦 - 飘移.flac",
            "music/game/JAY/周杰伦 - 龙卷风.flac",
        };
        int idx = rand() % gameSongs.size();
        SoundTool::getInstance()->playBackgroundMusic(
            const_cast<char*>(gameSongs[idx].c_str()));
        CocosDenshion::SimpleAudioEngine::getInstance()->setBackgroundMusicVolume(0.35f);
    }

    // 添加点击事件
    auto listener = EventListenerTouchOneByOne::create();
    listener->onTouchBegan = CC_CALLBACK_2(Game::touchCallBack, this);
    _eventDispatcher->addEventListenerWithSceneGraphPriority(listener, this);

    if (_inputMode == InputMode::TOUCH) {
        this->startShakeHookAnimation();
    } else {
        this->schedule(CC_SCHEDULE_SELECTOR(Game::updateGestureAngle), 0.05);
        this->schedule(CC_SCHEDULE_SELECTOR(Game::updateCameraPreview), 0.1);
    }
    schedule(CC_SCHEDULE_SELECTOR(Game::updateTime), 1, 59, 0);
}

void Game::startShakeHookAnimation()
{
    if (_inputMode != InputMode::TOUCH) return;

    rope->setRotation(0);
    
    float duration = 1;
    float angle = 65;
    rope->runAction(RepeatForever::create(Sequence::create(RotateTo::create(duration, angle), RotateTo::create(duration, 0), RotateTo::create(duration, -angle), RotateTo::create(duration, 0), NULL)));
}

void Game::stopGame()
{
    // 停止一切时间
    rope->stopAllActions();
    this->stopAllActions();
    this->unscheduleAllCallbacks();

    // Restore gesture-mode background music
    if (_inputMode != InputMode::TOUCH) {
        SoundTool::getInstance()->playBackgroundMusic("music/backMusic-exchange.flac");
        CocosDenshion::SimpleAudioEngine::getInstance()->setBackgroundMusicVolume(0.5f);
    }

    SoundTool::getInstance()->playEffect("music/finish.mp3");
    
    // 判断获取的分数是否能过关
    if (passScroe > (UserDataManager::getInstance()->getAllMoney() - curPayMoney + curStageScore)) {
        // 分数不够
        StageFailOrSucessLayer::showTips(this, StageFailOrSucessLayer::TipsType::FAIL, curStageScore, 0);
    } else {
        // Pass
        StageFailOrSucessLayer::showTips(this, StageFailOrSucessLayer::TipsType::SUCESS, curStageScore, curPayMoney);
    }
}

bool Game::touchCallBack(cocos2d::Touch *touch, cocos2d::Event *event)
{
    if (!ropeChangeing) {
        
        SoundTool::getInstance()->playEffect("music/lastart.mp3");
        
        rope->stopAllActions();
        ropeChangeing = true;
        minerTimeLine->gotoFrameAndPlay(0, 105, true);
        schedule(CC_SCHEDULE_SELECTOR(Game::addRopeHeight), 0.025);
    }
    
    return false;
}

void Game::addRopeHeight(float dt)
{
    middleCircle->setPosition(circlePosition);
    ropeHeight += 10;
    rope->setContentSize(Size(3, ropeHeight));
}

void Game::addButtonAction(Node *csbNode)
{
    Button *settingBtn = static_cast<Button *>(Helper::seekWidgetByName(static_cast<Widget *>(csbNode), "settingButton"));
    settingBtn->addTouchEventListener([=](Ref *sender, Widget::TouchEventType type){
        if (type == Widget::TouchEventType::ENDED) {
            this->unschedule(CC_SCHEDULE_SELECTOR(Game::updateTime));
            _isPaused = true;
            Pause::showPause(Director::getInstance()->getRunningScene(), passScroe <= (UserDataManager::getInstance()->getAllMoney() - curPayMoney + curStageScore));
        }
    });
}

void Game::stopShakeHookAnimation()
{
    rope->stopActionByTag(100);
    rope->setRotation(0);
}

void Game::setupModePanel(Node *parent)
{
    auto panel = Layout::create();
    panel->setContentSize(Size(kWinSizeWidth, 40));
    panel->setAnchorPoint(Vec2::ANCHOR_MIDDLE);
    panel->setPosition(Vec2(kWinSizeWidth * 0.5f, kWinSizeHeight - 30));
    panel->setLayoutType(Layout::Type::HORIZONTAL);
    panel->setBackGroundColorType(Layout::BackGroundColorType::SOLID);
    panel->setBackGroundColor(Color3B(30, 30, 30));
    panel->setBackGroundColorOpacity(180);
    panel->setName("ModePanel");
    parent->addChild(panel, 100);

    auto createModeBtn = [&](const std::string& text, InputMode mode) -> Button* {
        auto btn = Button::create("Resources/button.png");
        btn->setTitleText(text);
        btn->setTitleFontSize(20);
        btn->setTitleColor(Color3B::BLACK);
        btn->setScale9Enabled(true);
        btn->setContentSize(Size(100, 35));
        btn->addTouchEventListener([this, mode](Ref*, Widget::TouchEventType type) {
            if (type == Widget::TouchEventType::ENDED) {
                switchInputMode(mode);
            }
        });
        return btn;
    };

    auto touchBtn = createModeBtn("TOUCH", InputMode::TOUCH);
    touchBtn->setName("touchBtn");
    panel->addChild(touchBtn);

    auto opencvBtn = createModeBtn("OPENCV", InputMode::OPENCV);
    opencvBtn->setName("opencvBtn");
    panel->addChild(opencvBtn);

    auto aiBtn = createModeBtn("AI", InputMode::AI);
    aiBtn->setName("aiBtn");
    panel->addChild(aiBtn);

    auto label = Text::create("TOUCH", "", 16);
    label->setPosition(Vec2(kWinSizeWidth * 0.5f, kWinSizeHeight - 55));
    label->setName("modeLabel");
    parent->addChild(label, 100);
    _modeLabel = label;

    _modePanel = panel;

    // Set initial highlight
    auto* activeBtn = static_cast<Button*>(panel->getChildByName("touchBtn"));
    if (activeBtn) activeBtn->setColor(Color3B(100, 200, 100));
}

void Game::switchInputMode(InputMode mode)
{
    if (_inputMode == mode) return;
    InputMode oldMode = _inputMode;
    _inputMode = mode;

    // Update button highlights
    if (_modePanel) {
        for (const char* name : {"touchBtn", "opencvBtn", "aiBtn"}) {
            auto* btn = static_cast<Button*>(_modePanel->getChildByName(name));
            if (!btn) continue;
            bool active = false;
            if (mode == InputMode::TOUCH && strcmp(name, "touchBtn") == 0) active = true;
            if (mode == InputMode::OPENCV && strcmp(name, "opencvBtn") == 0) active = true;
            if (mode == InputMode::AI && strcmp(name, "aiBtn") == 0) active = true;
            btn->setColor(active ? Color3B(100, 200, 100) : Color3B::WHITE);
        }
    }

    // Stop current mode
    this->unschedule(CC_SCHEDULE_SELECTOR(Game::updateGestureAngle));
    this->unschedule(CC_SCHEDULE_SELECTOR(Game::updateCameraPreview));
    rope->stopActionByTag(100);

    if (mode == InputMode::TOUCH) {
        if (_modeLabel) _modeLabel->setString("TOUCH");
        GestureClient::getInstance()->disconnect();
        if (_cameraPreview) {
            _cameraPreview->setVisible(false);
        }
        this->startShakeHookAnimation();
    } else {
        if (_modeLabel) {
            _modeLabel->setString(mode == InputMode::OPENCV ? "OPENCV" : "AI");
        }
        // Only launch server and connect when first entering gesture mode
        if (oldMode == InputMode::TOUCH) {
            auto* gc = GestureClient::getInstance();
            gc->launchServer();
            gc->connectHttp("http://localhost:5000");
            gc->setCallback([this](const GestureData& data) {
                this->onGestureData(data);
            });
        }

        this->schedule(CC_SCHEDULE_SELECTOR(Game::updateGestureAngle), 0.05);
        this->schedule(CC_SCHEDULE_SELECTOR(Game::updateCameraPreview), 0.1);
    }
}

void Game::updateGestureAngle(float dt)
{
    if (_inputMode == InputMode::TOUCH) return;

    GestureData data = GestureClient::getInstance()->getData();
    // Only control angle during swing phase — lock direction when hook is extended
    if (data.connected && !ropeChangeing && !isOpenHook) {
        _gestureAngle = data.angle;
        rope->setRotation(_gestureAngle);
    }
}

void Game::updateCameraPreview(float dt)
{
    if (_inputMode == InputMode::TOUCH) return;

    GestureData data = GestureClient::getInstance()->getData();
    if (!data.hasNewFrame || data.jpegFrame.empty()) return;

    Image* img = new (std::nothrow) Image();
    if (!img->initWithImageData(data.jpegFrame.data(), data.jpegFrame.size())) {
        delete img;
        return;
    }

    Texture2D* tex = new (std::nothrow) Texture2D();
    if (!tex->initWithImage(img)) {
        delete img;
        delete tex;
        return;
    }

    if (!_cameraPreview) {
        _cameraPreview = Sprite::create();
        if (!_cameraPreview) {
            delete tex;
            delete img;
            return;
        }
        _cameraPreview->setPosition(Vec2(kWinSizeWidth - 100, kWinSizeHeight - 120));
        _cameraPreview->setScale(0.4f);
        _cameraPreview->setGlobalZOrder(200);
        this->addChild(_cameraPreview, 200);
    }

    _cameraPreview->setTexture(tex);
    _cameraPreview->setVisible(true);

    tex->release();
    img->release();

    // In AI mode, also send frame to AI service
    if (_inputMode == InputMode::AI) {
        AIGestureService::getInstance()->sendFrame(data.jpegFrame);
    }
}

void Game::onGestureData(const GestureData& data)
{
    // FIST (握拳) → drop hook; OPEN_PALM (张开手掌) → aim (handled in updateGestureAngle)
    if (data.gesture == "FIST" && !ropeChangeing && !isOpenHook) {
        SoundTool::getInstance()->playEffect("music/lastart.mp3");

        rope->stopAllActions();
        ropeChangeing = true;
        minerTimeLine->gotoFrameAndPlay(0, 105, true);
        schedule(CC_SCHEDULE_SELECTOR(Game::addRopeHeight), 0.025);
    }
}