#include "Mineral.hpp"

USING_NS_CC;

Mineral* Mineral::create(const std::string& name, float scaleX, float scaleY,
                          float rotate, bool potion, bool diamonds, bool stoneBook) {
    auto* m = new Mineral();
    if (m && m->init(name, scaleX, scaleY, rotate, potion, diamonds, stoneBook)) {
        m->autorelease();
        return m;
    }
    delete m;
    return nullptr;
}

bool Mineral::init(const std::string& name, float scaleX, float scaleY,
                    float rotate, bool potion, bool diamonds, bool stoneBook) {
    // Strip .csb suffix to get image name
    std::string imageName;
    for (char c : name) {
        if (c == '.') break;
        imageName += c;
    }
    imageName += ".png";

    if (!Sprite::initWithSpriteFrameName(imageName))
        return false;

    if (potion)  _power = 1.2f;
    if (diamonds) _diamondsCoe = 3;
    if (stoneBook) _stoneCoe = 3;

    int scale = (int)(scaleX * 100);
    setScale(scaleX, scaleY);
    setRotation(rotate);
    setAnchorPoint(Vec2(0.5f, 0.5f));

    // --- Classification & stats ---
    if (name == "gold-0-0.png" && scale == 40) {
        mineralType = Type::GOLD_SMALL;
        score = 100;  backSpeed = 3 * _power;  hookRote = 16;
        setPosition(7.52f, -21.24f);
    } else if (name == "gold-0-0.png" && scale == 65) {
        mineralType = Type::GOLD_MEDIUM;
        score = 200;  backSpeed = 2 * _power;  hookRote = 36;
        setPosition(2.86f, -36.33f);
    } else if (name == "pulled-gold-1-0.png" && scale == 40) {
        mineralType = Type::GOLD_SMALL;
        score = 100;  backSpeed = 3 * _power;  hookRote = 12;
        setPosition(7.81f, -24.17f);
    } else if (name == "pulled-gold-1-0.png" && scale == 65) {
        mineralType = Type::GOLD_MEDIUM;
        score = 200;  backSpeed = 2 * _power;  hookRote = 25;
        setPosition(8.51f, -35.43f);
    } else if (name == "pulled-gold-0-0.png" && scale == 90) {
        mineralType = Type::GOLD_LARGE;
        score = 400;  backSpeed = 1.5f * _power;  hookRote = 35;
        setPosition(5.2f, -48.17f);
    } else if (name == "gold-1-6.png" && scale == 90) {
        mineralType = Type::GOLD_LARGE;
        score = 400;  backSpeed = 1.5f * _power;  hookRote = 35;
        setPosition(10.83f, -44.7f);
    } else if (name == "gold-0-1.png" && scale == 65) {
        mineralType = Type::GOLD_MEDIUM;
        score = 200;  backSpeed = 2 * _power;  hookRote = 30;
        setPosition(5.66f, -34.47f);
    } else if (name == "treasure-bag.png") {
        mineralType = Type::TREASURE_BAG;
        score = rand() % 200 + 50;  backSpeed = 3 * _power;  hookRote = 5;
        setPosition(6.31f, -33.65f);
    } else if (name == "stone-0.png" && scale == 80) {
        mineralType = Type::STONE_SMALL;
        score = 25 * _stoneCoe;  backSpeed = 3 * _power;  hookRote = 15;
        setPosition(5.8f, -26.07f);
    } else if (name == "stone-1.png" && scale == 100) {
        mineralType = Type::STONE_MEDIUM;
        score = 50 * _stoneCoe;  backSpeed = 2 * _power;  hookRote = 30;
        setPosition(8.56f, -29.8f);
    } else if (name == "stone-0.png" && scale == 150) {
        mineralType = Type::STONE_LARGE;
        score = 75 * _stoneCoe;  backSpeed = 1.5f * _power;  hookRote = 30;
        setPosition(5.27f, -42.01f);
    } else if (name == "diamond.png") {
        mineralType = Type::DIAMOND;
        score = 500 * _diamondsCoe;  backSpeed = 3 * _power;  hookRote = 6;
        setPosition(6.38f, -26.91f);
    } else {
        mineralType = Type::STONE_SMALL;
        score = 0;  backSpeed = 10;
    }

    return true;
}
