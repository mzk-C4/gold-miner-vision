#include "MainWindow.h"
#include "scene/SceneManager.h"
#include <QVBoxLayout>
#include <QStatusBar>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUI();
    setupToolBar();
    setupStatusBar();
}

MainWindow::~MainWindow()
{
    // SceneManager 会在析构时停止手势线程
}

void MainWindow::setupUI()
{
    // 创建 QGraphicsView
    m_view = new QGraphicsView(this);
    m_view->setRenderHint(QPainter::Antialiasing);
    m_view->setRenderHint(QPainter::SmoothPixmapTransform);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    m_view->setFrameStyle(QFrame::NoFrame);

    setCentralWidget(m_view);

    // 创建场景管理器
    m_sceneManager = new SceneManager(m_view, this);
}

void MainWindow::setupToolBar()
{
    auto *toolbar = addToolBar("模式");
    toolbar->setMovable(false);

    toolbar->addWidget(new QLabel("视觉模式: "));

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem("键盘操作（兜底）");
#ifdef HAS_OPENCV
    m_modeCombo->addItem("本地CV（颜色手套）");
#endif
    m_modeCombo->addItem("AI视觉（豆包API）");

    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onModeChanged);

    toolbar->addWidget(m_modeCombo);

    // 模式说明标签
    m_modeLabel = new QLabel("  当前: 键盘模式");
    toolbar->addWidget(m_modeLabel);
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage("就绪 | 空格放钩 | ←→控制方向 | B炸药 | Esc返回");
}

void MainWindow::onModeChanged(int index)
{
    switch (index) {
    case 0:
        m_sceneManager->setVisionMode(SceneManager::ModeKeyboard);
        m_modeLabel->setText("  当前: 键盘模式");
        break;
    case 1:
        m_sceneManager->setVisionMode(SceneManager::ModeLocalCV);
        m_modeLabel->setText("  当前: 本地CV模式");
        break;
    case 2:
        m_sceneManager->setVisionMode(SceneManager::ModeAIVision);
        m_modeLabel->setText("  当前: AI视觉模式");
        break;
    }
}
