//
//  UserDataManager.hpp
//  GoldMiner
//
//  Created by sfbest on 2016/11/28.
//
//

#ifndef UserDataManager_hpp
#define UserDataManager_hpp

#include <stdio.h>
#include "Const.hpp"
#include "SoundTool.hpp"
#include "services/PlayerManager.hpp"

class UserDataManager {

public:
    static UserDataManager *getInstance();
    bool getMusicMute() {
        return SoundTool::getInstance()->getMusicIsMute();
    };
    
    bool getSoundMute() {
        return SoundTool::getInstance()->getEffectIsMute();
    };
    
    long getAllMoney() {
        return _allMoney;
    };
    
    int getStageNum() {
        return _stageNum;
    }
    
    void setMusicMute(bool mute);
    void setSoundMute(bool mute);
    void setAllMoney(long allMoney) {
        _allMoney = allMoney;
    };
    
    void setStageNum(int stageNum) {
        _stageNum = stageNum;
    };
    
    void saveUserData() {
        PlayerManager::getInstance()->setAllMoney(_allMoney);
        PlayerManager::getInstance()->setStageNum(_stageNum);
        PlayerManager::getInstance()->saveProfile();
    };
    
private:
    bool _musicMute;
    bool _soundMute;
    long _allMoney;
    int _stageNum;
};

#endif /* UserDataManager_hpp */
