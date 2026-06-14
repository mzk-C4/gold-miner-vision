//
//  SoundTool.cpp
//  MyGame
//
//  Created by sfbest on 16/8/31.
//
//

#include "SoundTool.hpp"
#include "SimpleAudioEngine.h"
#include <iostream>

#define kMusicMute "kMusicMute"
#define kEffectMute "kEffectMute"

static SoundTool *s_SharedSoundTool = nullptr;

SoundTool *SoundTool::getInstance()
{
    if (!s_SharedSoundTool) {
        s_SharedSoundTool = new SoundTool();
    }
    
    return s_SharedSoundTool;
}

SoundTool::SoundTool()
{
    _musicStatus = Stop;
    UserDefault::getInstance()->setBoolForKey(kMusicMute, false);
    UserDefault::getInstance()->setBoolForKey(kEffectMute, false);
    _musicMute = false;
    _effectMute = false;
}

void SoundTool::setEffectMute(bool mute)
{
    if (_effectMute == mute) return;
    _effectMute = mute;
    UserDefault::getInstance()->setBoolForKey(kEffectMute, mute);
}

void SoundTool::setMusicMute(bool mute)
{
    if (_musicMute == mute) return;
    _musicMute = mute;
    UserDefault::getInstance()->setBoolForKey(kMusicMute, mute);
    
    if (mute) {
        if (_musicStatus == Playing || _musicStatus == Pause) {
            CocosDenshion::SimpleAudioEngine::getInstance()->pauseBackgroundMusic();
            _musicStatus = Pause;
        }
    } else {
        if (_musicStatus == Pause) {
            CocosDenshion::SimpleAudioEngine::getInstance()->resumeBackgroundMusic();
            _musicStatus = Playing;
        } else if (_musicStatus == Stop) {
            CocosDenshion::SimpleAudioEngine::getInstance()->playBackgroundMusic(_lastMusic.c_str(), true);
            _musicStatus = Playing;
        }
    }
}

void SoundTool::playBackgroundMusic(char* pszFilePath)
{
    if (_musicMute) {
        _lastMusic = std::string(pszFilePath);
        return;
    }
    
    switch (_musicStatus) {
        case Stop:
            CocosDenshion::SimpleAudioEngine::getInstance()->playBackgroundMusic(pszFilePath, true);
            break;
        case Pause:
            
            if (_lastMusic == std::string(pszFilePath)) {
                CocosDenshion::SimpleAudioEngine::getInstance()->resumeBackgroundMusic();
            } else {
                CocosDenshion::SimpleAudioEngine::getInstance()->playBackgroundMusic(pszFilePath, true);
            }

            break;
        case Playing:
            if (_lastMusic == std::string(pszFilePath)) {
                break;
            } else {
                CocosDenshion::SimpleAudioEngine::getInstance()->playBackgroundMusic(pszFilePath, true);
            }
            break;
        default:
            break;
    }
    
    _lastMusic = std::string(pszFilePath);
    _musicStatus = Playing;
}

void SoundTool::playEffect(char *pszFilePath)
{
    if (_effectMute) {
        return;
    }
    
    CocosDenshion::SimpleAudioEngine::getInstance()->playEffect(pszFilePath);

//    typedef map<std::string,Value>::value_type valType;
//    _soundIDs.erase(pszFilePath);
//    _soundIDs.insert(valType(pszFilePath, Value(id)));

}

void SoundTool::stopEffect(char *pszFilePath)
{
    unsigned int id = _soundIDs[pszFilePath].asInt();
    CocosDenshion::SimpleAudioEngine::getInstance()->stopEffect(id);
}


