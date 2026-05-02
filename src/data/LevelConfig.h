/**
 * LevelConfig — 关卡配置加载器
 * 从 JSON 文件读取关卡数据，替代原项目的 .csb 文件
 */
#ifndef LEVELCONFIG_H
#define LEVELCONFIG_H

#include <QString>
#include <QVector>

struct MineralConfig {
    QString type;
    qreal   x = 0;
    qreal   y = 0;
    int     value  = 0;
    int     weight = 10;
    qreal   size   = 30;
};

struct LevelData {
    int level       = 1;
    int targetMoney = 650;
    int timeLimit   = 60;
    QVector<MineralConfig> minerals;
};

class LevelConfig
{
public:
    static LevelData loadLevel(int stageNum);
    static int totalLevels();

private:
    static QString configFilePath(int stageNum);
};

#endif // LEVELCONFIG_H
