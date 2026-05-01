#include "LevelConfig.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>

LevelData LevelConfig::loadLevel(int stageNum)
{
    LevelData data;
    data.level       = stageNum;
    data.targetMoney = 650;
    data.timeLimit   = 60;

    QString path = configFilePath(stageNum);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        // 配置文件不存在时返回基础配置
        return data;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) return data;

    QJsonObject root = doc.object();
    data.targetMoney = root.value("target_money").toInt(650);
    data.timeLimit   = root.value("time_limit").toInt(60);

    QJsonArray arr = root.value("minerals").toArray();
    for (const auto &val : arr) {
        QJsonObject obj = val.toObject();
        MineralConfig mc;
        mc.type   = obj.value("type").toString("stone");
        mc.x      = obj.value("x").toDouble(0);
        mc.y      = obj.value("y").toDouble(0);
        mc.value  = obj.value("value").toInt(0);
        mc.weight = obj.value("weight").toInt(10);
        mc.size   = obj.value("size").toDouble(30);
        data.minerals.append(mc);
    }

    return data;
}

int LevelConfig::totalLevels()
{
    // 检查 config 目录下有多少个 level*.json 文件
    QString configDir = "config";
    int count = 0;
    for (int i = 1; ; ++i) {
        if (QFile::exists(configFilePath(i)))
            count++;
        else
            break;
    }
    return qMax(count, 5); // 至少5关
}

QString LevelConfig::configFilePath(int stageNum)
{
    return QString("config/level%1.json").arg(stageNum);
}
