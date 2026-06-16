#include "MainWindow.h"
#include "NewProjectDialog.h"
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
#include <QFile>
#include <QTimer>
#include <QSettings>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), m_settingsWindow(nullptr)
{
    setWindowTitle("Avionix Designer");
    setObjectName("MainWindow");
    
    //разрешение на вложенные dock-виджеты и анимацию
    setDockNestingEnabled(true);
    setAnimated(true);
    setDockOptions(dockOptions() | QMainWindow::AllowTabbedDocks | QMainWindow::GroupedDragging);
    
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
    
    //автозагрузка последнего проекта
    if (settings.value("autoLoad", false).toBool()) {
        QString lastProject = settings.value("lastProject").toString();
        if (!lastProject.isEmpty() && QFile::exists(lastProject)) {
            ProjectManager::instance()->loadFromFile(lastProject);
            updateWindowTitle();
        }
    }
    
    //занять весь допустимый экран
    showMaximized();
}

void MainWindow::onNewProject()
{
    if (!m_newProjectDialog) {
        m_newProjectDialog = new NewProjectDialog(this);
    }

    if (m_newProjectDialog->exec() != QDialog::Accepted)
        return;

    const QString name = m_newProjectDialog->projectName();
    const QString filePath = m_newProjectDialog->filePath();

    ProjectManager::instance()->createNewProject(
        name,
        m_newProjectDialog->canvasWidth(),
        m_newProjectDialog->canvasHeight(),
        m_newProjectDialog->backgroundColor(),
        filePath
    );

    if (!filePath.isEmpty()) {
        ProjectManager::instance()->saveToFile(filePath);
        QSettings settings("Avionix", "Designer");
        settings.setValue("lastProject", filePath);
    }

    m_objectList->refreshList();
    m_objectList->selectRow(-1);
    m_objectProperties->clearProperties();
    m_viewport->setSelectedIndex(-1);
    m_viewport->resetView();
    updateWindowTitle();
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
        
        //сохраняем путь к последнему открытому проекту
        QSettings settings("Avionix", "Designer");
        settings.setValue("lastProject", fileName);
        
        updateWindowTitle();
    }
}

void MainWindow::onSaveFile()
{
    auto pm = ProjectManager::instance();
    if (pm->getFilePath().isEmpty()) {
        onSaveFileAs();
        return;
    }

    pm->saveToFile();
}

void MainWindow::onSaveFileAs()
{
    auto pm = ProjectManager::instance();

    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Сохранить проект как...",
        pm->getFilePath().isEmpty() ? pm->getProjectName() + ".xml" : pm->getFilePath(),
        "XML Files (*.xml)"
    );
    
    if (!fileName.isEmpty()) {
        pm->saveToFile(fileName);
        QSettings settings("Avionix", "Designer");
        settings.setValue("lastProject", fileName);
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
        connect(m_settingsWindow, &SettingsWindow::settingsReset, this, &MainWindow::resetToDefaultLayout);
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
    m_viewportDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    
    //ObjectListPanel
    m_objectListDock = new QDockWidget("Список объектов", this);
    m_objectListDock->setObjectName("ObjectListDock");
    m_objectListDock->setWidget(m_objectList);
    m_objectListDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_objectListDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    
    //ObjectPropertiesPanel
    m_objectPropertiesDock = new QDockWidget("Свойства объекта", this);
    m_objectPropertiesDock->setObjectName("ObjectPropertiesDock");
    m_objectPropertiesDock->setWidget(m_objectProperties);
    m_objectPropertiesDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_objectPropertiesDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    
    //ViewportSettingsPanel
    m_viewportSettingsDock = new QDockWidget("Настройки холста", this);
    m_viewportSettingsDock->setObjectName("ViewportSettingsDock");
    m_viewportSettingsDock->setWidget(m_viewportSettings);
    m_viewportSettingsDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_viewportSettingsDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    
    //ObjectLibraryPanel
    m_objectLibraryDock = new QDockWidget("Библиотека объектов", this);
    m_objectLibraryDock->setObjectName("ObjectLibraryDock");
    m_objectLibraryDock->setWidget(m_objectLibrary);
    m_objectLibraryDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_objectLibraryDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    
    //расположение панелей
    addDockWidget(Qt::LeftDockWidgetArea, m_viewportDock);
    addDockWidget(Qt::RightDockWidgetArea, m_objectListDock);
    
    addDockWidget(Qt::RightDockWidgetArea, m_objectPropertiesDock);
    splitDockWidget(m_objectListDock, m_objectPropertiesDock, Qt::Vertical);
    
    addDockWidget(Qt::BottomDockWidgetArea, m_viewportSettingsDock);
    addDockWidget(Qt::BottomDockWidgetArea, m_objectLibraryDock);
    tabifyDockWidget(m_viewportSettingsDock, m_objectLibraryDock);
    m_viewportSettingsDock->raise();
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
    int viewportWidth = static_cast<int>(windowW * 0.76);
    int rightPanelWidth = static_cast<int>(windowW * 0.24);
    resizeDocks({m_viewportDock}, {viewportWidth}, Qt::Horizontal);
    resizeDocks({m_objectListDock}, {rightPanelWidth}, Qt::Horizontal);
    
    //высота правых частей
    int objectListHeight = static_cast<int>(windowH * 0.38);
    int objectPropsHeight = static_cast<int>(windowH * 0.42);
    resizeDocks({m_objectListDock, m_objectPropertiesDock}, {objectListHeight, objectPropsHeight}, Qt::Vertical);
    
    //высота нижних частей
    int bottomHeight = static_cast<int>(windowH * 0.13);
    resizeDocks({m_viewportDock, m_viewportSettingsDock}, {windowH - bottomHeight, bottomHeight}, Qt::Vertical);
}

void MainWindow::createMenus()
{
    //меню "Файл"
    QMenu *fileMenu = menuBar()->addMenu("Файл");
    
    fileMenu->addAction(createAction("Создать...", QKeySequence::New, this, SLOT(onNewProject())));
    fileMenu->addAction(createAction("Открыть...", QKeySequence::Open, this, SLOT(onOpenFile())));
    fileMenu->addAction(createAction("Сохранить", QKeySequence::Save, this, SLOT(onSaveFile())));
    fileMenu->addAction(createAction("Сохранить как...", QKeySequence::SaveAs, this, SLOT(onSaveFileAs())));
    
    fileMenu->addSeparator();
    
    fileMenu->addAction(createAction("Выход", QKeySequence::Quit, this, SLOT(close())));

    QMenu *objectsMenu = menuBar()->addMenu("Объекты");
    auto addObjectAction = [this, objectsMenu](const QString &title, const QKeySequence &shortcut, const QString &typeName) {
        QAction *action = objectsMenu->addAction(title);
        action->setShortcut(shortcut);
        connect(action, &QAction::triggered, this, [this, typeName]() {
            createObjectOfType(typeName);
        });
    };
    addObjectAction("Прямоугольник", QKeySequence("Alt+1"), "rectangle");
    addObjectAction("Static", QKeySequence("Alt+2"), "staticgroup");
    addObjectAction("Rotation Group", QKeySequence("Alt+3"), "rotationobject");
    addObjectAction("Авиагоризонт", QKeySequence("Alt+4"), "aviagorizont");
    addObjectAction("Текст", QKeySequence("Alt+5"), "text");
    
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
    
    settingsMenu->addAction(createAction("Параметры...", QKeySequence("Ctrl+,"), this, SLOT(openSettings())));
}

void MainWindow::connectSignals()
{
    //связь списка объектов с панелью свойств
    connect(m_objectList, &ObjectListPanel::objectSelected, m_objectProperties, &ObjectPropertiesPanel::showObjectProperties);
    connect(m_objectList, &ObjectListPanel::objectSelected, m_viewport, &ViewportPanel::setSelectedIndex);
    
    //связь холста со списком и свойствами
    connect(m_viewport, &ViewportPanel::objectSelected, m_objectList, &ObjectListPanel::selectRow);
    connect(m_viewport, &ViewportPanel::objectSelected, m_objectProperties, &ObjectPropertiesPanel::showObjectProperties);
    
    //связь загрузки проекта с обновлением заголовка
    connect(ProjectManager::instance(), &ProjectManager::projectLoaded, this, &MainWindow::updateWindowTitle);
    connect(ProjectManager::instance(), &ProjectManager::projectLoaded, m_objectProperties, &ObjectPropertiesPanel::clearProperties);
    connect(ProjectManager::instance(), &ProjectManager::projectChanged, m_objectList, &ObjectListPanel::refreshList);
    connect(ProjectManager::instance(), &ProjectManager::projectChanged, m_viewport, QOverload<>::of(&QWidget::update));
    
    //связь изменения свойств с перерисовкой (без автосохранения в файл)
    connect(m_objectProperties, &ObjectPropertiesPanel::propertyChanged, m_viewport, QOverload<>::of(&QWidget::update));
    
    //связь изменений от манипуляторов с обновлением панели свойств
    connect(m_viewport, &ViewportPanel::objectChanged, this, [this]() {
        m_objectProperties->showObjectProperties(m_viewport->getSelectedIndex());
    });

    //создание объекта из библиотеки
    connect(m_objectLibrary, &ObjectLibraryPanel::objectRequested, this, &MainWindow::createObjectOfType);
    
    //связь изменения цвета фона с перерисовкой
    connect(m_viewportSettings, &ViewportSettingsPanel::bgColorChanged, m_viewport, QOverload<>::of(&QWidget::update));
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

void MainWindow::createObjectOfType(const QString &typeName)
{
    const int index = ProjectManager::instance()->addObject(typeName);
    if (index < 0)
        return;

    m_objectList->refreshList();
    m_objectList->selectRow(index);
    m_viewport->setSelectedIndex(index);
    m_objectProperties->showObjectProperties(index);
    m_viewport->update();
}

QAction* MainWindow::createAction(const QString &text, const QKeySequence &shortcut, const QObject *receiver, const char *member)
{
    QAction *action = new QAction(text, this);
    action->setShortcut(shortcut);
    connect(action, SIGNAL(triggered()), receiver, member);
    addAction(action);
    return action;
}
