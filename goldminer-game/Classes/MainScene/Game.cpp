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
#include "AIGestureRecognizer.hpp"
#include "GestureFusion.hpp"
#include "LevelLoader.hpp"
#include "platform/CCImage.h"
#include "base/CCEventListenerMouse.h"
#include "json/rapidjson.h"
#include "json/document-wrapper.h"
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
            this->detonateBomb();
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
    int stageNum = UserDataManager::getInstance()->getStageNum();

    // ── 优先 JSON 关卡系统（LevelLoader）──
    auto* levelLoader = LevelLoader::getInstance();
    if (!levelLoader->isLoaded()) {
        levelLoader->loadLevelFile("level_data.json");
    }

    if (levelLoader->isLoaded()) {
        const LevelDef* levelDef = levelLoader->getLevel(stageNum);
        if (levelDef) {
            // 更新目标分数（JSON 驱动）
            passScroe = levelDef->targetMoney;
            targetMoney->setString(to_string(passScroe));

            // 创建矿物容器
            auto goldsLayout = Layout::create();
            goldsLayout->setContentSize(Size(kWinSizeWidth, kWinSizeHeight));
            goldsLayout->setAnchorPoint(Vec2::ANCHOR_BOTTOM_LEFT);
            goldsLayout->setPosition(Vec2::ZERO);
            goldsLayout->setName("goldPanel");
            this->addChild(goldsLayout);

            for (const auto& minDef : levelDef->minerals) {
                std::string frameName = LevelLoader::typeToFrameName(minDef.type);
                auto sprite = Sprite::createWithSpriteFrameName(frameName);
                if (!sprite) continue;

                float px = minDef.x * kWinSizeWidth;
                float py = minDef.y * kWinSizeHeight;
                sprite->setPosition(px, py);
                sprite->setScale(minDef.scale);
                sprite->setName(frameName);
                goldsLayout->addChild(sprite);

                Size bodySize = Size(sprite->getContentSize().width * minDef.scale,
                                     sprite->getContentSize().height * minDef.scale);
                PhysicsBody* goldBody = PhysicsBody::createEdgeBox(bodySize);
                goldBody->setCategoryBitmask(10);
                goldBody->setCollisionBitmask(10);
                goldBody->setContactTestBitmask(10);
                sprite->addComponent(goldBody);
            }
            CCLOG("[Game] Loaded level %d from JSON (%d minerals)",
                  levelDef->level, (int)levelDef->minerals.size());
            return;
        }
    }

    // ── 兜底：CSB 关卡文件 ──
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

void Game::detonateBomb()
{
    if (!isOpenHook || !_hookedMineral) return;

    bompButton->setVisible(false);
    backSpeed = 10;

    isOpenHook = false;
    leftHook->setRotation(0);
    rightHook->setRotation(0);

    auto position = rope->convertToWorldSpace(middleCircle->getPosition());
    ParticleSystemQuad* bombEm = ParticleSystemQuad::create("Boom.plist");
    bombEm->setPosition(position);
    this->addChild(bombEm);

    SoundTool::getInstance()->playEffect("music/bomb.mp3");

    _hookedMineral->removeFromParent();
    _hookedMineral = nullptr;
}

void Game::onRightMouseClick(EventMouse* event)
{
    if (event->getMouseButton() == EventMouse::MouseButton::BUTTON_RIGHT) {
        this->detonateBomb();
    }
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

    // MusicPlayer handles background music — don't restart backMusic

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
    // 右键引爆（所有模式通用）
    auto mouseListener = EventListenerMouse::create();
    mouseListener->onMouseDown = CC_CALLBACK_1(Game::onRightMouseClick, this);
    _eventDispatcher->addEventListenerWithSceneGraphPriority(mouseListener, this);

    if (_inputMode == InputMode::TOUCH) {
        // 触摸模式：点击屏幕释放钩子
        auto listener = EventListenerTouchOneByOne::create();
        listener->onTouchBegan = CC_CALLBACK_2(Game::touchCallBack, this);
        _eventDispatcher->addEventListenerWithSceneGraphPriority(listener, this);
        this->startShakeHookAnimation();
    } else {
        // 姿态模式：手势控制钩子，触摸仅用于 UI 按钮（炸弹等）
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

void Game::switchInputMode(InputMode mode)
{
    if (_inputMode == mode) return;
    InputMode oldMode = _inputMode;
    _inputMode = mode;

    // Stop current mode
    this->unschedule(CC_SCHEDULE_SELECTOR(Game::updateGestureAngle));
    this->unschedule(CC_SCHEDULE_SELECTOR(Game::updateCameraPreview));
    rope->stopActionByTag(100);

    if (mode == InputMode::TOUCH) {
        // 关闭手势识别
        GestureFusion::getInstance()->shutdown();
        if (_cameraPreview) {
            _cameraPreview->setVisible(false);
        }
        this->startShakeHookAnimation();
    } else {
        if (oldMode == InputMode::TOUCH) {
            // ── 手势识别架构初始化 ──
            auto* fusion = GestureFusion::getInstance();
            bool initOk = false;

            if (mode == InputMode::AI) {
                // AI 模式：摄像头直连大模型，跳过 OpenCV/MediaPipe
                // 从配置文件读取 API 凭据
                std::string cloudEp = "doubao-seed-2-0-mini-260428";
                std::string cloudKey = "";
                std::string configPath = FileUtils::getInstance()->fullPathForFilename("ai_config.json");
                if (!configPath.empty()) {
                    std::string jsonData = FileUtils::getInstance()->getStringFromFile(configPath);
                    if (!jsonData.empty()) {
                        rapidjson::Document cfg;
                        cfg.Parse(jsonData.c_str());
                        if (!cfg.HasParseError()) {
                            if (cfg.HasMember("model"))  cloudEp = cfg["model"].GetString();
                            if (cfg.HasMember("apiKey")) cloudKey = cfg["apiKey"].GetString();
                        }
                    }
                }
                initOk = fusion->initializeAI(cloudEp, cloudKey);
                CCLOG("[Game] AI mode: camera -> LLM direct gesture recognition");
            } else {
                // OpenCV 模式：本地 GestureServer 手势检测
                initOk = fusion->initialize("", "");
                CCLOG("[Game] OpenCV mode: local GestureServer gesture recognition");
            }

            if (!initOk) {
                CCLOG("[Game] WARNING: GestureFusion init failed, falling back to touch");
                _inputMode = InputMode::TOUCH;
                this->startShakeHookAnimation();
                return;
            }

            // 注册手势指令回调（钩子释放 + 炸药引爆）
            fusion->setCommandCallback([this](const GestureCommand& cmd) {
                if (cmd.shouldReleaseHook && !ropeChangeing && !isOpenHook) {
                    CCLOG("[Game] GestureFusion → RELEASE HOOK (angle=%.1f)", cmd.targetAngle);
                    SoundTool::getInstance()->playEffect("music/lastart.mp3");
                    rope->stopAllActions();
                    ropeChangeing = true;
                    minerTimeLine->gotoFrameAndPlay(0, 105, true);
                    schedule(CC_SCHEDULE_SELECTOR(Game::addRopeHeight), 0.025);
                }
                if (cmd.shouldDetonateBomb) {
                    CCLOG("[Game] GestureFusion → DETONATE BOMB");
                    this->detonateBomb();
                }
            });
        }

        this->schedule(CC_SCHEDULE_SELECTOR(Game::updateGestureAngle), 0.05);
        this->schedule(CC_SCHEDULE_SELECTOR(Game::updateCameraPreview), 0.1);
    }
}

void Game::updateGestureAngle(float dt)
{
    if (_inputMode == InputMode::TOUCH) return;

    // 从融合器获取本帧指令（包含 EMA 平滑后的角度 + 钩子释放信号）
    GestureCommand cmd = GestureFusion::getInstance()->tick(dt);

    if (cmd.isValid && !ropeChangeing && !isOpenHook) {
        // 将融合器输出的角度应用到钩子旋转
        float target = cmd.targetAngle;
        float smooth = 0.25f;
        _gestureAngle += (target - _gestureAngle) * smooth;
        rope->setRotation(_gestureAngle);
    }
}

void Game::updateCameraPreview(float dt)
{
    if (_inputMode == InputMode::TOUCH) return;

    std::vector<uint8_t> jpegFrame;

    if (_inputMode == InputMode::AI) {
        // AI 模式：帧来自 AIGestureRecognizer（摄像头直连，无 OpenCV）
        jpegFrame = AIGestureRecognizer::getInstance()->getLatestFrame();
    } else {
        // OpenCV 模式：帧来自 GestureClient（GestureServer 摄像头）
        GestureData data = GestureClient::getInstance()->getData();
        if (data.hasNewFrame && !data.jpegFrame.empty())
            jpegFrame = std::move(data.jpegFrame);
    }

    if (jpegFrame.empty()) return;

    // 推送帧给融合器
    GestureFusion::getInstance()->pushPreviewFrame(jpegFrame);

    Image* img = new (std::nothrow) Image();
    if (!img->initWithImageData(jpegFrame.data(), jpegFrame.size())) {
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
        _cameraPreview->setPosition(Vec2(kWinSizeWidth - 70, kWinSizeHeight - 80));
        _cameraPreview->setScale(0.22f);
        _cameraPreview->setGlobalZOrder(200);
        this->addChild(_cameraPreview, 200);
    }

    _cameraPreview->setTexture(tex);
    _cameraPreview->setVisible(true);

    tex->release();
    img->release();
}

// onGestureData 已移除 — 钩子释放逻辑由 GestureFusion::setCommandCallback 处理