#include "LevelLoader.hpp"
#include "platform/CCFileUtils.h"
#include "json/rapidjson.h"
#include "json/document-wrapper.h"

USING_NS_CC;

static LevelLoader* s_levelLoader = nullptr;

LevelLoader* LevelLoader::getInstance() {
    if (!s_levelLoader) {
        s_levelLoader = new LevelLoader();
    }
    return s_levelLoader;
}

bool LevelLoader::loadLevelFile(const std::string& path) {
    _levels.clear();

    std::string fullPath = FileUtils::getInstance()->fullPathForFilename(path);
    std::string jsonData = FileUtils::getInstance()->getStringFromFile(fullPath);

    if (jsonData.empty()) {
        CCLOG("LevelLoader: Cannot read %s", path.c_str());
        return false;
    }

    rapidjson::Document doc;
    doc.Parse(jsonData.c_str());
    if (doc.HasParseError()) {
        CCLOG("LevelLoader: JSON parse error in %s", path.c_str());
        return false;
    }

    const auto& levelArr = doc["levels"];
    if (!levelArr.IsArray()) {
        CCLOG("LevelLoader: Missing 'levels' array in JSON");
        return false;
    }

    for (unsigned i = 0; i < levelArr.Size(); i++) {
        const auto& lv = levelArr[i];
        LevelDef def;

        def.level = lv["level"].GetInt();
        def.targetMoney = lv["targetMoney"].GetInt();
        def.timeLimit = lv.HasMember("timeLimit") ? lv["timeLimit"].GetInt() : 60;

        const auto& mins = lv["minerals"];
        if (mins.IsArray()) {
            for (unsigned j = 0; j < mins.Size(); j++) {
                const auto& m = mins[j];
                MineralDef md;
                md.type = m["type"].GetString();
                md.x = m["x"].GetFloat();
                md.y = m["y"].GetFloat();
                md.scale = m["scale"].GetFloat();
                def.minerals.push_back(md);
            }
        }

        _levels.push_back(def);
    }

    CCLOG("LevelLoader: Loaded %d levels from JSON", (int)_levels.size());
    return true;
}

const LevelDef* LevelLoader::getLevel(int levelNum) const {
    if (_levels.empty()) return nullptr;
    int index = levelNum % (int)_levels.size();
    if (index == 0) index = (int)_levels.size();
    return &_levels[index - 1];
}

std::string LevelLoader::typeToFrameName(const std::string& type) {
    // Map JSON type to sprite frame names (matching .csb names)
    if (type == "gold_small")       return "gold-0-0.png";
    if (type == "gold_medium")      return "gold-0-0.png";
    if (type == "gold_large")       return "pulled-gold-0-0.png";
    if (type == "stone_small")      return "stone-0.png";
    if (type == "stone_medium")     return "stone-1.png";
    if (type == "stone_large")      return "stone-0.png";
    if (type == "diamond")          return "diamond.png";
    if (type == "treasure_bag")     return "treasure-bag.png";
    return type + ".png";
}
