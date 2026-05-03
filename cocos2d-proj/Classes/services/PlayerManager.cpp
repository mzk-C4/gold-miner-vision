#include "PlayerManager.hpp"
#include "json/rapidjson.h"
#include "json/document-wrapper.h"
#include "json/writer.h"
#include "json/stringbuffer.h"
#include "base/CCUserDefault.h"
#include <sstream>
#include <iomanip>

USING_NS_CC;

static PlayerManager* s_playerMgr = nullptr;
static const char* DEFAULT_PLAYER = "Guest";

PlayerManager* PlayerManager::getInstance() {
    if (!s_playerMgr) s_playerMgr = new PlayerManager();
    return s_playerMgr;
}

PlayerManager::PlayerManager() {
    _currentPlayer = DEFAULT_PLAYER;
    _profile.name = DEFAULT_PLAYER;

    // Load existing profile or start fresh
    auto* ud = UserDefault::getInstance();
    std::string index = ud->getStringForKey(getIndexKey().c_str(), "");
    if (!index.empty()) {
        // Load first player in the list as default
        size_t pos = index.find(',');
        if (pos != std::string::npos) {
            std::string firstPlayer = index.substr(0, pos);
            loadProfile(firstPlayer);
        } else {
            loadProfile(index);
        }
    }
}

std::string PlayerManager::getIndexKey() const { return "player_index"; }
std::string PlayerManager::getProfileKey(const std::string& name) const {
    return "player_profile_" + name;
}

void PlayerManager::selectPlayer(const std::string& name) {
    loadProfile(name);
}

void PlayerManager::createPlayer(const std::string& name) {
    // Add to index
    auto* ud = UserDefault::getInstance();
    std::string index = ud->getStringForKey(getIndexKey().c_str(), "");
    if (!index.empty()) index += ",";
    index += name;
    ud->setStringForKey(getIndexKey().c_str(), index);

    // Create fresh profile
    _currentPlayer = name;
    _profile = PlayerProfile{};
    _profile.name = name;
    _profile.stageNum = 1;
    _profile.allMoney = 0;
    saveProfile();
}

void PlayerManager::deletePlayer(const std::string& name) {
    auto* ud = UserDefault::getInstance();

    // Remove from index
    std::string index = ud->getStringForKey(getIndexKey().c_str(), "");
    std::string newIndex;
    size_t pos = 0, prev = 0;
    while ((pos = index.find(',', prev)) != std::string::npos) {
        std::string p = index.substr(prev, pos - prev);
        if (p != name) {
            if (!newIndex.empty()) newIndex += ",";
            newIndex += p;
        }
        prev = pos + 1;
    }
    std::string last = index.substr(prev);
    if (last != name) {
        if (!newIndex.empty()) newIndex += ",";
        newIndex += last;
    }
    ud->setStringForKey(getIndexKey().c_str(), newIndex);

    // Remove profile data
    ud->setStringForKey(getProfileKey(name).c_str(), "");

    // If we deleted the current player, switch to first available or guest
    if (_currentPlayer == name) {
        std::vector<std::string> players = listPlayers();
        if (!players.empty()) selectPlayer(players[0]);
        else {
            _currentPlayer = DEFAULT_PLAYER;
            _profile = PlayerProfile{};
            _profile.name = DEFAULT_PLAYER;
        }
    }
}

std::vector<std::string> PlayerManager::listPlayers() {
    std::vector<std::string> result;
    std::string index = UserDefault::getInstance()->getStringForKey(getIndexKey().c_str(), "");
    if (index.empty()) return result;

    size_t pos = 0, prev = 0;
    std::string idx = index + ",";
    while ((pos = idx.find(',', prev)) != std::string::npos) {
        std::string name = idx.substr(prev, pos - prev);
        if (!name.empty()) result.push_back(name);
        prev = pos + 1;
    }
    return result;
}

void PlayerManager::addHistory(int stageNum, int score, int targetMoney, bool passed) {
    HistoryEntry entry;
    entry.stageNum = stageNum;
    entry.score = score;
    entry.targetMoney = targetMoney;
    entry.passed = passed;

    auto now = std::time(nullptr);
    std::tm tm;
    localtime_s(&tm, &now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M");
    entry.timestamp = oss.str();

    _profile.history.push_back(entry);
    // Keep only last 50 entries
    if (_profile.history.size() > 50)
        _profile.history.erase(_profile.history.begin());
    saveProfile();
}

void PlayerManager::saveProfile() {
    rapidjson::Document doc;
    doc.SetObject();
    auto& alloc = doc.GetAllocator();

    doc.AddMember("name", rapidjson::Value(_profile.name.c_str(), alloc), alloc);
    doc.AddMember("allMoney", (int64_t)_profile.allMoney, alloc);
    doc.AddMember("stageNum", _profile.stageNum, alloc);

    rapidjson::Value historyArr(rapidjson::kArrayType);
    for (const auto& h : _profile.history) {
        rapidjson::Value entry(rapidjson::kObjectType);
        entry.AddMember("stage", h.stageNum, alloc);
        entry.AddMember("score", h.score, alloc);
        entry.AddMember("target", h.targetMoney, alloc);
        entry.AddMember("passed", h.passed, alloc);
        entry.AddMember("time", rapidjson::Value(h.timestamp.c_str(), alloc), alloc);
        historyArr.PushBack(entry, alloc);
    }
    doc.AddMember("history", historyArr, alloc);

    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    doc.Accept(writer);

    UserDefault::getInstance()->setStringForKey(getProfileKey(_profile.name).c_str(), buf.GetString());
    UserDefault::getInstance()->flush();
}

void PlayerManager::loadProfile(const std::string& name) {
    std::string json = UserDefault::getInstance()->getStringForKey(getProfileKey(name).c_str(), "");
    if (json.empty()) {
        // New profile
        _currentPlayer = name;
        _profile = PlayerProfile{};
        _profile.name = name;
        _profile.stageNum = 1;
        _profile.allMoney = 0;
        saveProfile();
        return;
    }

    rapidjson::Document doc;
    doc.Parse(json.c_str());
    if (doc.HasParseError()) return;

    _currentPlayer = name;
    _profile = PlayerProfile{};
    _profile.name = doc["name"].GetString();
    _profile.allMoney = doc["allMoney"].GetInt64();
    _profile.stageNum = doc["stageNum"].GetInt();

    const auto& historyArr = doc["history"];
    if (historyArr.IsArray()) {
        for (unsigned i = 0; i < historyArr.Size(); i++) {
            const auto& h = historyArr[i];
            HistoryEntry entry;
            entry.stageNum = h["stage"].GetInt();
            entry.score = h["score"].GetInt();
            entry.targetMoney = h["target"].GetInt();
            entry.passed = h["passed"].GetBool();
            entry.timestamp = h["time"].GetString();
            _profile.history.push_back(entry);
        }
    }
}
