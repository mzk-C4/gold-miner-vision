#include "UserDataManager.h"

UserDataManager* UserDataManager::s_instance = nullptr;

UserDataManager* UserDataManager::getInstance()
{
    if (!s_instance)
        s_instance = new UserDataManager();
    return s_instance;
}

UserDataManager::UserDataManager(QObject *parent)
    : QObject(parent)
    , m_settings("GoldMiner", "GoldMiner")
{
    m_musicMuted = m_settings.value("musicMuted", false).toBool();
    m_soundMuted = m_settings.value("soundMuted", false).toBool();
    m_allMoney   = m_settings.value("allMoney", 0).toInt();
    m_stageNum   = m_settings.value("stageNum", 1).toInt();
}

void UserDataManager::setMusicMuted(bool mute)
{
    m_musicMuted = mute;
    m_settings.setValue("musicMuted", mute);
}

void UserDataManager::setSoundMuted(bool mute)
{
    m_soundMuted = mute;
    m_settings.setValue("soundMuted", mute);
}

void UserDataManager::setAllMoney(int money)
{
    m_allMoney = money;
    m_settings.setValue("allMoney", money);
}

void UserDataManager::setStageNum(int stage)
{
    m_stageNum = stage;
    m_settings.setValue("stageNum", stage);
}

bool UserDataManager::spendMoney(int amount)
{
    if (m_allMoney >= amount) {
        m_allMoney -= amount;
        m_settings.setValue("allMoney", m_allMoney);
        return true;
    }
    return false;
}

void UserDataManager::addMoney(int amount)
{
    m_allMoney += amount;
    m_settings.setValue("allMoney", m_allMoney);
}

void UserDataManager::advanceToNextStage()
{
    m_stageNum++;
    m_settings.setValue("stageNum", m_stageNum);
}
