/**
 * MainWindow — 主窗口
 * 包含 QGraphicsView + 场景管理器 + 模式切换工具栏
 */
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QGraphicsView;
class QComboBox;
class QLabel;
class QStatusBar;
class SceneManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onModeChanged(int index);

private:
    void setupUI();
    void setupToolBar();

    QGraphicsView *m_view = nullptr;
    SceneManager  *m_sceneManager = nullptr;
    QComboBox     *m_modeCombo = nullptr;
    QLabel        *m_modeLabel = nullptr;
};

#endif // MAINWINDOW_H
