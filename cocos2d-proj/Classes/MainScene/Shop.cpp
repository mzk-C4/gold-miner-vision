//
//  Shop.cpp
//  GoldMiner
//
//  Created by sfbest on 2016/12/6.
//
//

#include "Shop.hpp"
#include "UserDataManager.hpp"
#include "Game.hpp"
#include "2d/CCSpriteFrameCache.h"

#define kBombPrice 200
#define kPotionPrice 300
#define kDiamondsPrice 200
#define kStoneBookPrice 100

// Recursive search that traverses ALL children, not just Widgets.
// Helper::seekWidgetByName only traverses Widget* nodes, so it misses
// Sprite nodes (like "businessman") that are nested under other Sprites.
static Node* seekNodeByName(Node* root, const std::string& name)
{
    if (!root) return nullptr;
    if (root->getName() == name) return root;
    for (const auto& child : root->getChildren()) {
        Node* found = seekNodeByName(child, name);
        if (found) return found;
    }
    return nullptr;
}

Scene *Shop::createScene()
{
    Scene *scene = Scene::create();
    
    auto shopLayer = Shop::create();
    scene->addChild(shopLayer);
    
    return scene;
}

bool Shop::init()
{
    if (! Layer::init()) {
        return false;
    }
    
    auto csb = CSLoader::createNode("ShopScene.csb");
    this->addChild(csb);

    // Load the new sprite sheet and collect its frames
    SpriteFrameCache::getInstance()->addSpriteFramesWithFile(
        "Resources/shopper_new.plist",
        "Resources/shopper new.png");

    Vector<SpriteFrame*> newFrames;
    for (int i = 1; i <= 23; i++) {
        char name[32];
        snprintf(name, sizeof(name), "shopper_new%04d.png", i);
        auto* frame = SpriteFrameCache::getInstance()->getSpriteFrameByName(name);
        if (frame) {
            newFrames.pushBack(frame);
        }
    }

    // Replace the ORIGINAL shoper frames in SpriteFrameCache with our
    // new ones. The CSB's built-in ActionTimeline keeps running with
    // its original timing and sequence — only the texture data changes.
    if (!newFrames.empty()) {
        auto* frameCache = SpriteFrameCache::getInstance();
        for (int i = 1; i <= 38; i++) {
            char name[32];
            snprintf(name, sizeof(name), "shoper%04d.png", i);
            auto* oldFrame = frameCache->getSpriteFrameByName(name);
            if (oldFrame) {
                // Cycle new frames across old frame slots
                auto* newFrame = newFrames.at((i - 1) % newFrames.size());
                oldFrame->setTexture(newFrame->getTexture());
                oldFrame->setRect(newFrame->getRect());
                oldFrame->setRotated(newFrame->isRotated());
                oldFrame->setOffsetInPixels(newFrame->getOffsetInPixels());
                oldFrame->setOriginalSizeInPixels(newFrame->getOriginalSizeInPixels());
            }
        }
    }

    goodsDesVec.push_back(Value("炸药.购买以后,当抓到较重且金额不多的物品时,按下上方炸药即可炸毁物品,以便节省时间.功效为下一关"));
    goodsDesVec.push_back(Value("力量药水.购买以后,在下一关力量会增加,抓到物品后拉回速度会增加20%.功效为下一关"));
    goodsDesVec.push_back(Value("优质矿石.购买后在下一关中收购钻石的价格将变成原价格的3倍,但不保证下一关一定会有钻石~其效果为下一关"));
    goodsDesVec.push_back(Value("矿石收藏书.购买后下一关的矿石的价格将会是原有价格的3倍,其功效为下一关"));

    goodsDesText = static_cast<Text *>(Helper::seekWidgetByName(static_cast<Widget *>(csb), "shopDetail"));
    Text *userMoney = static_cast<Text *>(Helper::seekWidgetByName(static_cast<Widget *>(csb), "userMoney"));
    userMoney->setString("$" + to_string(UserDataManager::getInstance()->getAllMoney()));
    
    Button *buyButton = static_cast<Button *>(Helper::seekWidgetByName(static_cast<Widget *>(csb), "buyButton"));
    buyButton->addTouchEventListener([=](Ref *sender, Widget::TouchEventType type){
        if (type == Widget::TouchEventType::ENDED) {
            int index = lastSelected->getTag() - 1;
            
            auto oneIV = buyOnes.at(index);
            if (oneIV->isVisible()) {
                return;
            }
            // 获取商品价格
            int price = 0;
            switch (index) {
                case 0:
                    price = kBombPrice;
                    break;
                case 1:
                    price = kPotionPrice;
                    break;
                case 2:
                    price = kDiamondsPrice;
                    break;
                case 3:
                    price = kStoneBookPrice;
                    break;
            }
            
            if (UserDataManager::getInstance()->getAllMoney() - payMoneyCount - price >= 0) {
                kPlayClickSound
                payMoneyCount += price;
                userMoney->setString("$" + to_string(UserDataManager::getInstance()->getAllMoney() - payMoneyCount));
                oneIV->setVisible(true);
            }
        }
    });
    
    // addAction
    Button *nextButton = static_cast<Button *>(Helper::seekWidgetByName(static_cast<Widget *>(csb), "nextButton"));
    nextButton->addTouchEventListener([=](Ref *sender, Widget::TouchEventType type){
        if (type == Widget::TouchEventType::ENDED) {
            // next Stage
            ((Button *)sender)->setEnabled(false);
            kPlayClickSound
            Director::getInstance()->replaceScene(Game::createScene(buyOnes.at(0)->isVisible(), buyOnes.at(1)->isVisible(), buyOnes.at(2)->isVisible(), buyOnes.at(3)->isVisible(), payMoneyCount));
        }
    });

    for (int i = 1; i < 5; i++) {
        Button *goodsButton = static_cast<Button *>(Helper::seekWidgetByName(static_cast<Widget *>(csb), "goodsButton" + to_string(i)));
        goodsButton->setTag(i);
        goodsButton->addTouchEventListener(CC_CALLBACK_2(Shop::selectedGoodsButton, this));
        
        if (i == 1) {
            selectedGoodsButton(goodsButton, Widget::TouchEventType::ENDED);
        }
    }

    for (int i = 0; i < 4; i++) {
        ImageView *oneIV = static_cast<ImageView *>(Helper::seekWidgetByName(static_cast<Widget *>(csb), "buyOne" + to_string(i)));
        oneIV->setTag(i);
        buyOnes.pushBack(oneIV);
    }
    
    return true;
}

void Shop::selectedGoodsButton(cocos2d::Ref *sender, Widget::TouchEventType type)
{
    if (lastSelected == ((Button *)sender)) {
        return;
    }
    kPlayClickSound
    if (type == Widget::TouchEventType::ENDED) {
        lastSelected = ((Button *)sender);
        goodsDesText->setString(goodsDesVec.at(((Button *)sender)->getTag() - 1).asString());
    }
}
