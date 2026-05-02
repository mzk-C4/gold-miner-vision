#include "LevelConfig.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QCoreApplication>
#include <QDir>

LevelData LevelConfig::loadLevel(int stageNum)
{
    LevelData data;
    data.level       = stageNum;
    data.targetMoney = 650;
    data.timeLimit   = 60;

    QString path = configFilePath(stageNum);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return data;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) return data;

    QJsonObject root = doc.object();
    data.targetMoney = root.value("target_money").toInt(650);
    data.timeLimit   = root.value("time_limit").toInt(60);

    for (const auto &val : root.value("minerals").toArray()) {
        QJsonObject obj = val.toObject();
        MineralConfig mc;
        mc.type   = obj.value("type").toString();
        mc.x      = obj.value("x").toDouble();
        mc.y      = obj.value("y").toDouble();
        mc.value  = obj.value("value").toInt();
        mc.weight = obj.value("weight").toInt(10);
        mc.size   = obj.value("size").toDouble(30);
        data.minerals.append(mc);
    }

    return data;
}

int LevelConfig::totalLevels()
{
    for (int i = 1; ; ++i) {
        if (!QFile::exists(configFilePath(i)))
            return qMax(i - 1, 5);
    }
}

QString LevelConfig::configFilePath(int stageNum)
{
    // 优先从可执行文件同级目录读取，回退到相对路径
    QString exeDir = QCoreApplication::applicationDirPath();
    QString path = exeDir + "/config/level" + QString::number(stageNum) + ".json";
    if (QFile::exists(path))
        return path;
    return QString("config/level%1.json").arg(stageNum);
}
