/**
 * UserDataManager — 用户数据管理单例
 * 持久化：通过 QSettings 实现跨会话数据保存
 */
#ifndef USERDATAMANAGER_H
#define USERDATAMANAGER_H

#include <QObject>
#include <QSettings>

class UserDataManager : public QObject
{
    Q_OBJECT

public:
    static UserDataManager* getInstance();

    bool isMusicMuted() const { return m_musicMuted; }
    bool isSoundMuted() const { return m_soundMuted; }
    int  getAllMoney()  const { return m_allMoney; }
    int  getStageNum()  const { return m_stageNum; }

    void setMusicMuted(bool mute);
    void setSoundMuted(bool mute);
    void setAllMoney(int money);
    void setStageNum(int stage);

    bool spendMoney(int amount);
    void addMoney(int amount);
    void advanceToNextStage();

private:
    explicit UserDataManager(QObject *parent = nullptr);
    static UserDataManager* s_instance;

    QSettings m_settings;

    bool m_musicMuted = false;
    bool m_soundMuted = false;
    int  m_allMoney   = 0;
    int  m_stageNum   = 1;
};

#endif // USERDATAMANAGER_H
