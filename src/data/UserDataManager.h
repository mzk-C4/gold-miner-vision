/**
 * UserDataManager — 用户数据管理单例
 *
 * 职责：管理用户金币、关卡进度、音效/音乐开关
 * 持久化：通过 QSettings 实现跨会话数据保存
 * 参考：原项目 UserDataManager 单例模式
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

    // ========== 数据访问接口 ==========
    bool    isMusicMuted() const    { return m_musicMuted; }
    bool    isSoundMuted() const    { return m_soundMuted; }
    int     getAllMoney() const     { return m_allMoney; }
    int     getStageNum() const     { return m_stageNum; }

    void setMusicMuted(bool mute);
    void setSoundMuted(bool mute);
    void setAllMoney(int money);
    void setStageNum(int stage);

    // ========== 持久化 ==========
    void save();   // 立即写入 QSettings

    // ========== 游戏逻辑辅助 ==========
    bool spendMoney(int amount);     // 消费金币，返回是否成功
    void addMoney(int amount);       // 增加金币
    void advanceToNextStage();       // 关卡+1

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
