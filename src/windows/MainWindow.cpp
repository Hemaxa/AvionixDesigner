#include "MainWindow.h"
#include "SettingsWindow.h"
#include "ViewportPanel.h"
#include "ObjectListPanel.h"
#include "ObjectPropertiesPanel.h"
#include "ObjectLibraryPanel.h"
#include "ViewportSettingsPanel.h"
#include "ProjectManager.h"
#include "AppearanceManager.h"

#include <QDockWidget>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QScreen>
#include <QGuiApplication>
#include <QSplitter>
#include <QTimer>
#include <QSettings>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), m_settingsWindow(nullptr)
{
    setWindowTitle("Avionix Designer");
    setObjectName("MainWindow");
    
    //разрешение на вложенные dock-виджеты и анимацию
    setDockNestingEnabled(true);
    setAnimated(true);
    
    //применяем тему из настроек
    QSettings settings("Avionix", "Designer");
    int themeIndex = settings.value("theme", 0).toInt();
    auto am = AppearanceManager::instance();
    switch (themeIndex) {
        case 0: am->applyDarkTheme(); break;
        case 1: am->applyLightTheme(); break;
        case 2: am->applyAvionixTheme(); break;
        default: am->applyDarkTheme(); break;
    }
    
    //служебные методы создания
    createWidgets();
    createMenus();
    connectSignals();
    
    //восстановление layout из настроек
    restoreLayoutSettings();
    
    //занять весь допустимый экран
    showMaximized();
}

void MainWindow::onOpenFile()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Выберите XML файл",
        QString(),
        "XML Files (*.xml)"
    );
    
    //если путь не пуст, проект загружается
    if (!fileName.isEmpty()) {
        ProjectManager::instance()->loadFromFile(fileName);
        updateWindowTitle();
    }
}

void MainWindow::updateWindowTitle()
{
    //изменение заголовка окна в зависимости от имени открытого файла
    auto pm = ProjectManager::instance();
    setWindowTitle(QString("Avionix Designer - %1").arg(pm->getProjectName()));
}

void MainWindow::openSettings()
{
    //создаем окно настроек при первом вызове
    if (!m_settingsWindow) {
        m_settingsWindow = new SettingsWindow(this);
        //подключаем сигнал сброса настроек к сбросу layout
        connect(m_settingsWindow, &SettingsWindow::settingsReset, 
                this, &MainWindow::resetToDefaultLayout);
    }

    m_settingsWindow->show();
    m_settingsWindow->raise();
    m_settingsWindow->activateWindow();
}

void MainWindow::createWidgets()
{
    //инициализация панелей
    m_viewport = new ViewportPanel(this);
    m_objectList = new ObjectListPanel(this);
    m_objectProperties = new ObjectPropertiesPanel(this);
    m_objectLibrary = new ObjectLibraryPanel(this);
    m_viewportSettings = new ViewportSettingsPanel(this);
    
    //создание dock-виджетов
    //ViewportPanel
    m_viewportDock = new QDockWidget("Рабочая область", this);
    m_viewportDock->setObjectName("ViewportDock");
    m_viewportDock->setWidget(m_viewport);
    m_viewportDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_viewportDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    
    //ObjectListPanel
    m_objectListDock = new QDockWidget("Список объектов", this);
    m_objectListDock->setObjectName("ObjectListDock");
    m_objectListDock->setWidget(m_objectList);
    m_objectListDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    
    //ObjectPropertiesPanel
    m_objectPropertiesDock = new QDockWidget("Свойства объекта", this);
    m_objectPropertiesDock->setObjectName("ObjectPropertiesDock");
    m_objectPropertiesDock->setWidget(m_objectProperties);
    m_objectPropertiesDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    
    //ViewportSettingsPanel
    m_viewportSettingsDock = new QDockWidget("Настройки холста", this);
    m_viewportSettingsDock->setObjectName("ViewportSettingsDock");
    m_viewportSettingsDock->setWidget(m_viewportSettings);
    m_viewportSettingsDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    
    //ObjectLibraryPanel
    m_objectLibraryDock = new QDockWidget("Библиотека объектов", this);
    m_objectLibraryDock->setObjectName("ObjectLibraryDock");
    m_objectLibraryDock->setWidget(m_objectLibrary);
    m_objectLibraryDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    
    //расположение панелей
    addDockWidget(Qt::LeftDockWidgetArea, m_viewportDock);
    addDockWidget(Qt::RightDockWidgetArea, m_objectListDock);
    
    addDockWidget(Qt::RightDockWidgetArea, m_objectPropertiesDock);
    splitDockWidget(m_objectListDock, m_objectPropertiesDock, Qt::Vertical);
    
    addDockWidget(Qt::BottomDockWidgetArea, m_viewportSettingsDock);
    addDockWidget(Qt::BottomDockWidgetArea, m_objectLibraryDock);
    splitDockWidget(m_viewportSettingsDock, m_objectLibraryDock, Qt::Horizontal);
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    
    //применяем размеры только если нет сохранённого layout
    if (!m_initialSizesSet) {
        m_initialSizesSet = true;
        
        QSettings settings("Avionix", "Designer");
        if (!settings.contains("windowState")) {
            //отложенный вызов для корректного применения размеров
            QTimer::singleShot(0, this, &MainWindow::setupDockSizes);
        }
    }
}

void MainWindow::setupDockSizes()
{
    //получаем размеры окна для расчета процентов
    int windowW = width();
    int windowH = height();
    
    //ширина колонок
    int viewportWidth = static_cast<int>(windowW * 0.8);
    int rightPanelWidth = static_cast<int>(windowW * 0.2);
    resizeDocks({m_viewportDock}, {viewportWidth}, Qt::Horizontal);
    resizeDocks({m_objectListDock}, {rightPanelWidth}, Qt::Horizontal);
    
    //высота правых частей
    int objectListHeight = static_cast<int>(windowH * 0.4);
    int objectPropsHeight = static_cast<int>(windowH * 0.4);
    resizeDocks({m_objectListDock, m_objectPropertiesDock}, {objectListHeight, objectPropsHeight}, Qt::Vertical);
    
    //высота нижних частей
    int bottomHeight = static_cast<int>(windowH * 0.2);
    resizeDocks({m_viewportSettingsDock, m_objectLibraryDock}, {bottomHeight, bottomHeight}, Qt::Vertical);
    
    //ширина нижних частей
    int bottomPanelWidth = static_cast<int>(windowW * 0.375);
    resizeDocks({m_viewportSettingsDock, m_objectLibraryDock}, {bottomPanelWidth, bottomPanelWidth}, Qt::Horizontal);
}

void MainWindow::createMenus()
{
    //меню "Файл"
    QMenu *fileMenu = menuBar()->addMenu("Файл");
    
    QAction *openAction = fileMenu->addAction("Открыть...");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenFile);
    
    fileMenu->addSeparator();
    
    QAction *exitAction = fileMenu->addAction("Выход");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QMainWindow::close);
    
    //меню "Вид"
    QMenu *viewMenu = menuBar()->addMenu("Вид");
    
    //добавляем действия для переключения видимости каждой панели
    viewMenu->addAction(m_viewportDock->toggleViewAction());
    viewMenu->addAction(m_objectListDock->toggleViewAction());
    viewMenu->addAction(m_objectPropertiesDock->toggleViewAction());
    viewMenu->addAction(m_objectLibraryDock->toggleViewAction());
    viewMenu->addAction(m_viewportSettingsDock->toggleViewAction());
    
    viewMenu->addSeparator();
    
    QAction *resetViewAction = viewMenu->addAction("Сбросить масштаб");
    resetViewAction->setShortcut(QKeySequence("Ctrl+0"));
    connect(resetViewAction, &QAction::triggered, m_viewport, &ViewportPanel::resetView);
    
    //меню "Настройки"
    QMenu *settingsMenu = menuBar()->addMenu("Настройки");
    
    QAction *preferencesAction = settingsMenu->addAction("Параметры...");
    preferencesAction->setShortcut(QKeySequence("Ctrl+,"));
    connect(preferencesAction, &QAction::triggered, this, &MainWindow::openSettings);
}

void MainWindow::connectSignals()
{
    //связь списка объектов с панелью свойств
    connect(m_objectList, &ObjectListPanel::objectSelected, m_objectProperties, &ObjectPropertiesPanel::showObjectProperties);
    
    //связь загрузки проекта с обновлением заголовка
    connect(ProjectManager::instance(), &ProjectManager::projectLoaded, this, &MainWindow::updateWindowTitle);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    //сохраняем layout перед закрытием
    saveLayoutSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::saveLayoutSettings()
{
    QSettings settings("Avionix", "Designer");
    settings.setValue("windowGeometry", saveGeometry());
    settings.setValue("windowState", saveState());
}

void MainWindow::restoreLayoutSettings()
{
    QSettings settings("Avionix", "Designer");
    
    //проверяем, есть ли сохранённые настройки
    if (settings.contains("windowGeometry")) {
        restoreGeometry(settings.value("windowGeometry").toByteArray());
        restoreState(settings.value("windowState").toByteArray());
    }
}

void MainWindow::resetToDefaultLayout()
{
    //удаляем сохранённые настройки layout
    QSettings settings("Avionix", "Designer");
    settings.remove("windowGeometry");
    settings.remove("windowState");
    
    //применяем стандартные размеры
    showMaximized();
    setupDockSizes();
}
