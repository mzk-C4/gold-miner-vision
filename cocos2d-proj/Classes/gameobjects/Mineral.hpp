#pragma once

#include "cocos2d.h"

class Mineral : public cocos2d::Sprite {
public:
    enum class Type {
        GOLD_SMALL,
        GOLD_MEDIUM,
        GOLD_LARGE,
        STONE_SMALL,
        STONE_MEDIUM,
        STONE_LARGE,
        DIAMOND,
        TREASURE_BAG
    };

    static Mineral* create(const std::string& name, float scaleX, float scaleY,
                           float rotate, bool potion, bool diamonds, bool stoneBook);

    virtual bool init(const std::string& name, float scaleX, float scaleY,
                      float rotate, bool potion, bool diamonds, bool stoneBook);

    int score = 0;
    float hookRote = 0;
    int backSpeed = 10;
    Type mineralType;

protected:
    int _power = 1;
    int _stoneCoe = 1;
    int _diamondsCoe = 1;
};
