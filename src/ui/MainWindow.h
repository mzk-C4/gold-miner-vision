/**
 * MainWindow — 主窗口
 *
 * 包含 QGraphicsView + 场景管理器 + 模式切换工具栏
 * 手势识别工作线程在此启动和停止
 */
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsView>
#include <QToolBar>
#include <QComboBox>
#include <QLabel>

class SceneManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onModeChanged(int index);  // 视觉模式切换

private:
    void setupUI();
    void setupToolBar();
    void setupStatusBar();

    QGraphicsView *m_view         = nullptr;
    SceneManager  *m_sceneManager = nullptr;
    QComboBox     *m_modeCombo    = nullptr;
    QLabel        *m_modeLabel    = nullptr;
};

#endif // MAINWINDOW_H
