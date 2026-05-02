#include "MainWindow.h"
#include "scene/SceneManager.h"
#include <QGraphicsView>
#include <QComboBox>
#include <QLabel>
#include <QToolBar>
#include <QStatusBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("黄金矿工 — 手势控制版");
    setupUI();
    setupToolBar();
    statusBar()->showMessage("就绪 | 空格放钩 | ←→控制方向 | B炸药 | Esc返回");
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI()
{
    m_view = new QGraphicsView(this);
    m_view->setRenderHint(QPainter::Antialiasing);
    m_view->setRenderHint(QPainter::SmoothPixmapTransform);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    m_view->setFrameStyle(QFrame::NoFrame);
    m_view->setFixedSize(800, 600);

    setCentralWidget(m_view);
    setFixedSize(800, 640); // 留出工具栏和状态栏高度

    m_sceneManager = new SceneManager(m_view, this);
}

void MainWindow::setupToolBar()
{
    auto *toolbar = addToolBar("模式");
    toolbar->setMovable(false);
    toolbar->setStyleSheet("QToolBar{background:#333;padding:2px;}");

    auto *label = new QLabel(" 视觉模式: ");
    label->setStyleSheet("color:white;font-size:13px;");
    toolbar->addWidget(label);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->setStyleSheet(
        "QComboBox{background:#555;color:white;border:1px solid #777;"
        "padding:3px 8px;border-radius:3px;}"
        "QComboBox::drop-down{border:none;}"
        "QComboBox QAbstractItemView{background:#555;color:white;"
        "selection-background:#2a6496;}");
    m_modeCombo->addItem("键盘操作（兜底）");
#ifdef HAS_OPENCV
    m_modeCombo->addItem("本地CV（颜色手套）");
#endif
    m_modeCombo->addItem("AI视觉（豆包API）");

    // Qt 6: QComboBox::currentIndexChanged 只有一个 int 重载
    connect(m_modeCombo, &QComboBox::currentIndexChanged,
            this, &MainWindow::onModeChanged);
    toolbar->addWidget(m_modeCombo);

    m_modeLabel = new QLabel("  当前: 键盘模式  ");
    m_modeLabel->setStyleSheet("color:#aaa;font-size:12px;");
    toolbar->addWidget(m_modeLabel);
}

void MainWindow::onModeChanged(int index)
{
    if (index == 0) {
        m_sceneManager->setVisionMode(SceneManager::ModeKeyboard);
        m_modeLabel->setText("  当前: 键盘模式  ");
    }
#ifdef HAS_OPENCV
    else if (index == 1) {
        m_sceneManager->setVisionMode(SceneManager::ModeLocalCV);
        m_modeLabel->setText("  当前: 本地CV模式  ");
    }
    else if (index == 2) {
        m_sceneManager->setVisionMode(SceneManager::ModeAIVision);
        m_modeLabel->setText("  当前: AI视觉模式  ");
    }
#else
    else if (index == 1) {
        m_sceneManager->setVisionMode(SceneManager::ModeAIVision);
        m_modeLabel->setText("  当前: AI视觉模式  ");
    }
#endif
}
