//
//  Game.hpp
//  GoldMiner
//
//  Created by MacBook on 16/11/27.
//
//

#ifndef Game_hpp
#define Game_hpp

#include <stdio.h>
#include "Const.hpp"
#include "Mineral.hpp"
#include "GestureClient.hpp"

enum class InputMode { TOUCH, OPENCV, AI };

class Game : public Layer {
    
public:
    static Scene *createScene(bool isBuyBomb, bool isBuyPotion, bool isBuyDiamonds, bool isStoneBook, int payMoney);
    virtual bool init(bool isBuyBomb, bool isBuyPotion, bool isBuyDiamonds, bool isStoneBook, int payMoney);
    static Game *create(bool isBuyBomb, bool isBuyPotion, bool isBuyDiamonds, bool isStoneBook, int payMoney);
    static void setDefaultInputMode(InputMode mode) { _defaultInputMode = mode; }
    static InputMode getDefaultInputMode() { return _defaultInputMode; }

private:
    Text *allMoney;
    Text *targetMoney;
    Text *stageNum;
    Text *time;
    long passScroe;
    bool showStageTips;
    ImageView *rope;
    Sprite *middleCircle;
    Sprite *leftHook;
    Sprite *rightHook;
    
    cocostudio::timeline::ActionTimeline *minerTimeLine;
    
    int timeCount;
    int backSpeed = 10;
    int curStageScore;
    bool ropeChangeing;
    int ropeHeight = 20;
    int curPayMoney;

    Point circlePosition;
    bool isOpenHook;
    bool _isPaused = false;

    Mineral *_hookedMineral;

    bool isBuyPotion;
    bool isBuyDiamonds;
    bool isBuyStoneBook;
    Button *bompButton;

    // Gesture / input mode
    static InputMode _defaultInputMode;
    InputMode _inputMode = InputMode::TOUCH;
    Sprite *_cameraPreview = nullptr;
    float _gestureAngle = 0.0f;
    
private:
    void addButtonAction(Node *csbNode);
    void onEnter();
    void setUpText(Widget *csb);
    
    void startGame();
    void stopGame();
    
    void updateTime(float dt);
    
    void startShakeHookAnimation();
    void stopShakeHookAnimation();

    void addRopeHeight(float dt);
    void subRopeHeight(float dt);

    void loadStageInfo();

    bool touchCallBack(Touch *touch, Event *event);
    bool physicsBegin(PhysicsContact &contact);
    void pullGold(PhysicsContact &contact);

    // Gesture mode
    void switchInputMode(InputMode mode);
    void updateGestureAngle(float dt);
    void updateCameraPreview(float dt);
};

#endif /* Game_hpp */
