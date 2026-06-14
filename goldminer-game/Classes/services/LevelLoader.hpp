#pragma once

#include "cocos2d.h"
#include <vector>
#include <string>

struct MineralDef {
    std::string type;    // "gold_small", "gold_medium", "gold_large",
                         // "stone_small", "stone_medium", "stone_large",
                         // "diamond", "treasure_bag"
    float x, y;          // normalized position (0.0-1.0)
    float scale;         // display scale
};

struct LevelDef {
    int level = 1;
    int targetMoney = 650;
    int timeLimit = 60;
    std::vector<MineralDef> minerals;
};

class LevelLoader {
public:
    static LevelLoader* getInstance();
    bool loadLevelFile(const std::string& path);

    const LevelDef* getLevel(int levelNum) const;
    int getLevelCount() const { return (int)_levels.size(); }
    bool isLoaded() const { return !_levels.empty(); }

    /// Map JSON type string to the sprite frame name used in CSB
    static std::string typeToFrameName(const std::string& type);

private:
    LevelLoader() = default;
    std::vector<LevelDef> _levels;
};
