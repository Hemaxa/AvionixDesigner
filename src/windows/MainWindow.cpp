/**
 * @file MainWindow.cpp
 * @brief Реализация главного окна
 */

#include "MainWindow.h"
#include "SettingsWindow.h"
#include "../panels/ViewportPanel.h"
#include "../panels/ObjectListPanel.h"
#include "../panels/ObjectPropertiesPanel.h"
#include "../panels/ObjectLibraryPanel.h"
#include "../panels/ViewportSettingsPanel.h"
#include "../managers/ProjectManager.h"
#include "../managers/AppearanceManager.h"
#include <QDockWidget>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QTimer>
#include <QSplitter>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_settingsWindow(nullptr)
{
    setWindowTitle("Avionix Designer");
    setObjectName("MainWindow");
    
    // Применяем тёмную тему по умолчанию
    AppearanceManager::instance()->applyDarkTheme();
    
    createWidgets();
    createMenus();
    connectSignals();
    
    showMaximized();
    
    // Открываем файл при запуске
    QTimer::singleShot(100, this, &MainWindow::onOpenFile);
}

void MainWindow::onOpenFile()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Выберите XML файл",
        QString(),
        "XML Files (*.xml)"
    );
    
    if (!fileName.isEmpty()) {
        ProjectManager::instance()->loadFromFile(fileName);
        updateWindowTitle();
    }
}

void MainWindow::updateWindowTitle()
{
    auto pm = ProjectManager::instance();
    setWindowTitle(QString("Avionix Designer - %1").arg(pm->getProjectName()));
}

void MainWindow::openSettings()
{
    // Создаём окно настроек при первом вызове
    if (!m_settingsWindow) {
        m_settingsWindow = new SettingsWindow(this);
    }
    m_settingsWindow->show();
    m_settingsWindow->raise();
    m_settingsWindow->activateWindow();
}

void MainWindow::createWidgets()
{
    // ===== Центральная область: Viewport + нижние панели =====
    
    // Основной сплиттер (вертикальный) - верх/низ
    QSplitter *mainSplitter = new QSplitter(Qt::Vertical, this);
    mainSplitter->setObjectName("MainSplitter");
    
    // Холст (основная панель ~70% площади)
    m_viewport = new ViewportPanel(this);
    mainSplitter->addWidget(m_viewport);
    
    // Нижняя часть: ViewportSettings + ObjectLibrary
    QSplitter *bottomSplitter = new QSplitter(Qt::Horizontal, this);
    bottomSplitter->setObjectName("BottomSplitter");
    
    m_viewportSettings = new ViewportSettingsPanel(this);
    m_objectLibrary = new ObjectLibraryPanel(this);
    
    bottomSplitter->addWidget(m_viewportSettings);
    bottomSplitter->addWidget(m_objectLibrary);
    
    // Пропорции нижней панели 1:2
    bottomSplitter->setStretchFactor(0, 1);
    bottomSplitter->setStretchFactor(1, 2);
    
    mainSplitter->addWidget(bottomSplitter);
    
    // Пропорции верх/низ 3:1
    mainSplitter->setStretchFactor(0, 3);
    mainSplitter->setStretchFactor(1, 1);
    
    setCentralWidget(mainSplitter);
    
    // ===== Правая боковая панель (dock) =====
    
    // Сплиттер для правой стороны
    QSplitter *rightSplitter = new QSplitter(Qt::Vertical, this);
    rightSplitter->setObjectName("RightSplitter");
    
    // Список объектов (сверху справа)
    m_objectList = new ObjectListPanel(this);
    rightSplitter->addWidget(m_objectList);
    
    // Свойства объекта (снизу справа)
    m_objectProperties = new ObjectPropertiesPanel(this);
    rightSplitter->addWidget(m_objectProperties);
    
    // Равные пропорции
    rightSplitter->setStretchFactor(0, 1);
    rightSplitter->setStretchFactor(1, 1);
    
    // Dock для правой панели
    m_objectListDock = new QDockWidget("Панели", this);
    m_objectListDock->setObjectName("RightPanelDock");
    m_objectListDock->setWidget(rightSplitter);
    m_objectListDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
    m_objectListDock->setMinimumWidth(250);
    addDockWidget(Qt::RightDockWidgetArea, m_objectListDock);
}

void MainWindow::createMenus()
{
    // ===== Меню "Файл" =====
    QMenu *fileMenu = menuBar()->addMenu("Файл");
    
    QAction *openAction = fileMenu->addAction("Открыть...");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenFile);
    
    fileMenu->addSeparator();
    
    QAction *exitAction = fileMenu->addAction("Выход");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QMainWindow::close);
    
    // ===== Меню "Вид" =====
    QMenu *viewMenu = menuBar()->addMenu("Вид");
    
    viewMenu->addAction(m_objectListDock->toggleViewAction());
    
    viewMenu->addSeparator();
    
    QAction *resetViewAction = viewMenu->addAction("Сбросить масштаб");
    resetViewAction->setShortcut(QKeySequence("Ctrl+0"));
    connect(resetViewAction, &QAction::triggered, m_viewport, &ViewportPanel::resetView);
    
    // ===== Меню "Настройки" =====
    QMenu *settingsMenu = menuBar()->addMenu("Настройки");
    
    QAction *preferencesAction = settingsMenu->addAction("Параметры...");
    preferencesAction->setShortcut(QKeySequence("Ctrl+,"));
    connect(preferencesAction, &QAction::triggered, this, &MainWindow::openSettings);
}

void MainWindow::connectSignals()
{
    // Связь списка объектов с панелью свойств
    connect(m_objectList, &ObjectListPanel::objectSelected,
            m_objectProperties, &ObjectPropertiesPanel::showObjectProperties);
    
    // Обновление заголовка при загрузке проекта
    connect(ProjectManager::instance(), &ProjectManager::projectLoaded,
            this, &MainWindow::updateWindowTitle);
}
