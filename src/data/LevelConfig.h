/**
 * LevelConfig — 关卡配置加载器
 *
 * 职责：从 JSON 文件读取关卡数据（矿物类型/位置/价值/重量等）
 * 替代原项目的 .csb 文件解析
 */
#ifndef LEVELCONFIG_H
#define LEVELCONFIG_H

#include <QString>
#include <QVector>
#include <QJsonObject>

// 单个矿物的配置数据
struct MineralConfig {
    QString type;   // "small_gold" | "big_gold" | "diamond" | "stone"
    qreal   x;
    qreal   y;
    int     value;
    int     weight;
    qreal   size;
};

// 一个关卡的全部配置
struct LevelData {
    int     level;
    int     targetMoney;
    int     timeLimit;
    QVector<MineralConfig> minerals;
};

class LevelConfig
{
public:
    // 加载指定关卡（1-indexed），失败返回空的 LevelData
    static LevelData loadLevel(int stageNum);

    // 关卡总数
    static int totalLevels();

private:
    static QString configFilePath(int stageNum);
};

#endif // LEVELCONFIG_H
