#pragma once

#include "cocos2d.h"
#include <vector>
#include <string>
#include <ctime>

struct HistoryEntry {
    int stageNum;
    int score;
    int targetMoney;
    bool passed;
    std::string timestamp;  // ISO date string
};

struct PlayerProfile {
    std::string name;
    long allMoney = 0;
    int stageNum = 1;
    std::vector<HistoryEntry> history;
};

class PlayerManager {
public:
    static PlayerManager* getInstance();

    /// Get current player name (empty = default/guest)
    const std::string& currentPlayer() const { return _currentPlayer; }

    /// Switch to a player profile
    void selectPlayer(const std::string& name);
    void createPlayer(const std::string& name);
    void deletePlayer(const std::string& name);

    /// List all saved player profiles
    std::vector<std::string> listPlayers();

    /// Add a game result to current player's history
    void addHistory(int stageNum, int score, int targetMoney, bool passed);

    /// Get history for current player
    std::vector<HistoryEntry> getHistory() const { return _profile.history; }

    /// Access current player data
    long getAllMoney() const { return _profile.allMoney; }
    void setAllMoney(long m) { _profile.allMoney = m; }
    int getStageNum() const { return _profile.stageNum; }
    void setStageNum(int n) { _profile.stageNum = n; }

    /// Persist current state
    void saveProfile();
    void loadProfile(const std::string& name);

private:
    PlayerManager();
    std::string getIndexKey() const;     // stores comma-separated player list
    std::string getProfileKey(const std::string& name) const;

    std::string _currentPlayer;
    PlayerProfile _profile;
};
