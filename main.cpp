/**
 * 手势控制黄金矿工 — 程序入口
 * 技术栈：Qt 6 + QGraphicsScene + OpenCV + 手势识别
 */
#include <QApplication>
#include <QDir>
#include "ui/MainWindow.h"
#include "data/UserDataManager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("GoldMiner");
    app.setApplicationVersion("1.0");

    // 设置工作目录为可执行文件所在目录（确保 config/ 相对路径正确）
    QDir::setCurrent(QCoreApplication::applicationDirPath());

    // 初始化用户数据（QSettings 持久化）
    UserDataManager::getInstance();

    MainWindow window;
    window.setWindowTitle("黄金矿工 — 手势控制版");
    window.resize(800, 600);
    window.show();

    return app.exec();
}
