//
//  UserDataManager.cpp
//  GoldMiner
//
//  Created by tianzhongtao on 2016/11/28.
//

#include "UserDataManager.hpp"

static UserDataManager *s_SharedUserDataManager = nullptr;

UserDataManager *UserDataManager::getInstance()
{
    if (s_SharedUserDataManager == nullptr) {
        s_SharedUserDataManager = new UserDataManager();

        s_SharedUserDataManager->_musicMute = SoundTool::getInstance()->getMusicIsMute();
        s_SharedUserDataManager->_soundMute = SoundTool::getInstance()->getEffectIsMute();
        // Delegate to PlayerManager for per-player progress
        s_SharedUserDataManager->_allMoney = PlayerManager::getInstance()->getAllMoney();
        s_SharedUserDataManager->_stageNum = PlayerManager::getInstance()->getStageNum();
    }

    return s_SharedUserDataManager;
}

void UserDataManager::setMusicMute(bool mute)
{
    _musicMute = mute;
    
    SoundTool::getInstance()->setMusicMute(mute);
}

void UserDataManager::setSoundMute(bool mute)
{
    _soundMute = mute;
    
    SoundTool::getInstance()->setEffectMute(mute);
}
