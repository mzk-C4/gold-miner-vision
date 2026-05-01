/**
 * 手势控制黄金矿工 — 程序入口
 *
 * 技术栈：Qt 6 + QGraphicsScene + OpenCV + 手势识别
 * 双模系统：本地CV模式 / AI视觉模式 / 键盘兜底
 */
#include <QApplication>
#include "ui/MainWindow.h"
#include "data/UserDataManager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("GoldMiner");
    app.setApplicationVersion("1.0");

    // 初始化用户数据（QSettings 持久化）
    UserDataManager::getInstance();

    MainWindow window;
    window.setWindowTitle("黄金矿工 — 手势控制版 v1.0");
    window.resize(800, 600);
    window.show();

    return app.exec();
}
