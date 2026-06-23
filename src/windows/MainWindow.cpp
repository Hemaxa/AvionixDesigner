#include "MainWindow.h"

#include "AppearanceManager.h"
#include "EditorWorkspacePanel.h"
#include "FpgaSchemaRegistry.h"
#include "NewProjectDialog.h"
#include "ObjectLibraryPanel.h"
#include "ObjectListPanel.h"
#include "ObjectPropertiesPanel.h"
#include "ProjectManager.h"
#include "SelectionToolStrip.h"
#include "SettingsWindow.h"
#include "ViewportPanel.h"
#include "ViewportSettingsPanel.h"

#include <QAction>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QTimer>

namespace {
constexpr int kLayoutStateVersion = 2;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("Avionix Designer");
    setObjectName("MainWindow");
    setDockNestingEnabled(true);
    setAnimated(true);
    setDockOptions(dockOptions() | QMainWindow::AllowTabbedDocks | QMainWindow::GroupedDragging);

    QSettings settings("Avionix", "Designer");
    AppearanceManager::instance()->applyAvionixTheme();

    createWidgets();
    createMenus();
    connectSignals();
    restoreLayoutSettings();

    if (settings.value("autoLoad", false).toBool()) {
        const QString lastProject = settings.value("lastProject").toString();
        if (!lastProject.isEmpty() && QFile::exists(lastProject)) {
            ProjectManager::instance()->loadFromFile(lastProject);
            updateWindowTitle();
        }
    }

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
    setSelectionState(-1);
    updateWindowTitle();
}

void MainWindow::onOpenFile()
{
    const QString fileName = QFileDialog::getOpenFileName(
        this,
        "Открыть проект или XML",
        QString(),
        "Avionix Designer (*.avd);;FPGA XML (*.xml);;Все поддерживаемые (*.avd *.xml)"
    );

    if (!fileName.isEmpty()) {
        ProjectManager::instance()->loadFromFile(fileName);
        QSettings settings("Avionix", "Designer");
        settings.setValue("lastProject", fileName);
        m_objectList->refreshList();
        m_objectList->selectRow(-1);
        m_viewport->setSelectedIndex(-1);
        m_viewport->resetView();
        updateWindowTitle();
    }
}

void MainWindow::onSaveFile()
{
    auto *project = ProjectManager::instance();
    if (project->getFilePath().isEmpty()) {
        onSaveFileAs();
        return;
    }

    project->saveToFile();
}

void MainWindow::onSaveFileAs()
{
    auto *project = ProjectManager::instance();

    const QString fileName = QFileDialog::getSaveFileName(
        this,
        "Сохранить проект как...",
        project->getFilePath().isEmpty() ? project->getProjectName() + ".avd" : project->getFilePath(),
        "Avionix Designer (*.avd);;FPGA XML (*.xml)"
    );

    if (!fileName.isEmpty()) {
        project->saveToFile(fileName);
        QSettings settings("Avionix", "Designer");
        settings.setValue("lastProject", fileName);
        updateWindowTitle();
    }
}

void MainWindow::onExportFpgaXml()
{
    auto *project = ProjectManager::instance();
    const QString fileName = QFileDialog::getSaveFileName(
        this,
        "Сохранить кадр для ПЛИС",
        project->getProjectName() + ".xml",
        "FPGA XML (*.xml)"
    );

    if (!fileName.isEmpty()) {
        project->exportToFpgaXml(fileName);
    }
}

void MainWindow::onImportImage()
{
    const QString fileName = QFileDialog::getOpenFileName(
        this,
        "Добавить изображение",
        QString(),
        "Изображения (*.png *.jpg *.jpeg *.bmp *.svg)"
    );

    if (fileName.isEmpty())
        return;

    const int index = ProjectManager::instance()->importImageAsStaticGroup(fileName);
    if (index < 0)
        return;

    m_objectList->refreshList();
    m_objectList->selectRow(index);
    m_viewport->setSelectedIndex(index);
    m_objectProperties->showObjectProperties(index);
    setSelectionState(index);
    m_viewport->update();
}

void MainWindow::updateWindowTitle()
{
    auto *project = ProjectManager::instance();
    setWindowTitle(QString("Avionix Designer - %1 [%2]").arg(project->getProjectName(), project->editModeName()));
}

void MainWindow::openSettings()
{
    if (!m_settingsWindow) {
        m_settingsWindow = new SettingsWindow(this);
        connect(m_settingsWindow, &SettingsWindow::settingsReset, this, &MainWindow::resetToDefaultLayout);
    }

    m_settingsWindow->show();
    m_settingsWindow->raise();
    m_settingsWindow->activateWindow();
}

void MainWindow::createWidgets()
{
    m_workspacePanel = new EditorWorkspacePanel(this);
    setCentralWidget(m_workspacePanel);

    m_viewport = m_workspacePanel->viewport();
    m_selectionToolStrip = m_workspacePanel->selectionToolStrip();
    m_objectList = new ObjectListPanel(this);
    m_objectProperties = new ObjectPropertiesPanel(this);
    m_objectLibrary = new ObjectLibraryPanel(this);
    m_viewportSettings = new ViewportSettingsPanel(this);

    m_objectListDock = new QDockWidget("Список объектов", this);
    m_objectListDock->setObjectName("ObjectListDock");
    m_objectListDock->setWidget(m_objectList);
    m_objectListDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_objectListDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    m_objectPropertiesDock = new QDockWidget("Свойства объекта", this);
    m_objectPropertiesDock->setObjectName("ObjectPropertiesDock");
    m_objectPropertiesDock->setWidget(m_objectProperties);
    m_objectPropertiesDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_objectPropertiesDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    m_viewportSettingsDock = new QDockWidget("Холст", this);
    m_viewportSettingsDock->setObjectName("ViewportSettingsDock");
    m_viewportSettingsDock->setWidget(m_viewportSettings);
    m_viewportSettingsDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_viewportSettingsDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    m_objectLibraryDock = new QDockWidget("Объекты", this);
    m_objectLibraryDock->setObjectName("ObjectLibraryDock");
    m_objectLibraryDock->setWidget(m_objectLibrary);
    m_objectLibraryDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_objectLibraryDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

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

    if (!m_initialSizesSet) {
        m_initialSizesSet = true;
        QSettings settings("Avionix", "Designer");
        if (!settings.contains("windowState")) {
            QTimer::singleShot(0, this, &MainWindow::setupDockSizes);
        }
    }
}

void MainWindow::setupDockSizes()
{
    const int windowW = width();
    const int windowH = height();

    const int rightPanelWidth = qMax(300, static_cast<int>(windowW * 0.25));
    resizeDocks({m_objectListDock}, {rightPanelWidth}, Qt::Horizontal);

    const int objectListHeight = static_cast<int>(windowH * 0.32);
    const int objectPropsHeight = static_cast<int>(windowH * 0.48);
    resizeDocks({m_objectListDock, m_objectPropertiesDock}, {objectListHeight, objectPropsHeight}, Qt::Vertical);

    const int bottomHeight = qBound(96, static_cast<int>(windowH * 0.11), 144);
    resizeDocks({m_viewportSettingsDock}, {bottomHeight}, Qt::Vertical);
    resizeDocks(
        {m_viewportSettingsDock, m_objectLibraryDock},
        {static_cast<int>(windowW * 0.30), static_cast<int>(windowW * 0.54)},
        Qt::Horizontal
    );
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu("Файл");
    fileMenu->addAction(createAction("Создать...", QKeySequence::New, this, SLOT(onNewProject())));
    fileMenu->addAction(createAction("Открыть...", QKeySequence::Open, this, SLOT(onOpenFile())));
    fileMenu->addAction(createAction("Сохранить", QKeySequence::Save, this, SLOT(onSaveFile())));
    fileMenu->addAction(createAction("Сохранить как...", QKeySequence::SaveAs, this, SLOT(onSaveFileAs())));
    fileMenu->addAction(createAction("Сохранить кадр для ПЛИС...", QKeySequence("Ctrl+E"), this, SLOT(onExportFpgaXml())));
    fileMenu->addAction(createAction("Добавить изображение...", QKeySequence("Ctrl+I"), this, SLOT(onImportImage())));
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

    int shortcutIndex = 1;
    for (const auto &descriptor : FpgaSchemaRegistry::instance()->editorObjectCatalog()) {
        if (!descriptor.creatableInMenu)
            continue;
        addObjectAction(descriptor.title, QKeySequence(QString("Alt+%1").arg(shortcutIndex)), descriptor.typeName);
        ++shortcutIndex;
    }

    QMenu *viewMenu = menuBar()->addMenu("Вид");
    viewMenu->addAction(m_objectListDock->toggleViewAction());
    viewMenu->addAction(m_objectPropertiesDock->toggleViewAction());
    viewMenu->addAction(m_objectLibraryDock->toggleViewAction());
    viewMenu->addAction(m_viewportSettingsDock->toggleViewAction());
    viewMenu->addSeparator();

    QAction *resetViewAction = viewMenu->addAction("Сбросить масштаб");
    resetViewAction->setShortcut(QKeySequence("Ctrl+0"));
    connect(resetViewAction, &QAction::triggered, m_viewport, &ViewportPanel::resetView);

    QAction *deleteAction = new QAction("Удалить объект", this);
    deleteAction->setShortcut(QKeySequence::Delete);
    connect(deleteAction, &QAction::triggered, this, &MainWindow::deleteSelectedObject);
    addAction(deleteAction);

    QMenu *settingsMenu = menuBar()->addMenu("Настройки");
    settingsMenu->addAction(createAction("Параметры...", QKeySequence("Ctrl+,"), this, SLOT(openSettings())));
}

void MainWindow::connectSignals()
{
    connect(m_objectList, &ObjectListPanel::objectSelected, m_objectProperties, &ObjectPropertiesPanel::showObjectProperties);
    connect(m_objectList, &ObjectListPanel::objectSelected, m_viewport, &ViewportPanel::setSelectedIndex);
    connect(m_objectList, &ObjectListPanel::objectSelected, this, &MainWindow::setSelectionState);

    connect(m_viewport, &ViewportPanel::objectSelected, m_objectList, &ObjectListPanel::selectRow);
    connect(m_viewport, &ViewportPanel::objectSelected, m_objectProperties, &ObjectPropertiesPanel::showObjectProperties);
    connect(m_viewport, &ViewportPanel::objectSelected, this, &MainWindow::setSelectionState);

    connect(ProjectManager::instance(), &ProjectManager::projectLoaded, this, &MainWindow::updateWindowTitle);
    connect(ProjectManager::instance(), &ProjectManager::projectLoaded, m_objectProperties, &ObjectPropertiesPanel::clearProperties);
    connect(ProjectManager::instance(), &ProjectManager::projectLoaded, this, [this]() { setSelectionState(-1); });
    connect(ProjectManager::instance(), &ProjectManager::projectChanged, m_objectList, &ObjectListPanel::refreshList);
    connect(ProjectManager::instance(), &ProjectManager::projectChanged, m_viewport, QOverload<>::of(&QWidget::update));

    connect(m_objectProperties, &ObjectPropertiesPanel::propertyChanged, m_viewport, QOverload<>::of(&QWidget::update));
    connect(m_viewport, &ViewportPanel::objectChanged, this, [this]() {
        m_objectProperties->showObjectProperties(m_viewport->getSelectedIndex());
    });

    connect(m_objectLibrary, &ObjectLibraryPanel::objectRequested, this, &MainWindow::createObjectOfType);
    connect(m_objectLibrary, &ObjectLibraryPanel::imageImportRequested, this, &MainWindow::onImportImage);
    connect(m_selectionToolStrip, &SelectionToolStrip::alignRequested, this, &MainWindow::alignSelectedObject);
    connect(m_selectionToolStrip, &SelectionToolStrip::deleteRequested, this, &MainWindow::deleteSelectedObject);
    connect(m_viewport, &ViewportPanel::imageDropped, this, [this](const QString &fileName) {
        const int index = ProjectManager::instance()->importImageAsStaticGroup(fileName);
        if (index < 0)
            return;

        m_objectList->refreshList();
        m_objectList->selectRow(index);
        m_viewport->setSelectedIndex(index);
        m_objectProperties->showObjectProperties(index);
        setSelectionState(index);
        m_viewport->update();
    });
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveLayoutSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::saveLayoutSettings()
{
    QSettings settings("Avionix", "Designer");
    settings.setValue("windowGeometry", saveGeometry());
    settings.setValue("windowState", saveState(kLayoutStateVersion));
}

void MainWindow::restoreLayoutSettings()
{
    QSettings settings("Avionix", "Designer");
    if (settings.contains("windowGeometry")) {
        restoreGeometry(settings.value("windowGeometry").toByteArray());
        if (!restoreState(settings.value("windowState").toByteArray(), kLayoutStateVersion)) {
            settings.remove("windowGeometry");
            settings.remove("windowState");
        }
    }
}

void MainWindow::resetToDefaultLayout()
{
    QSettings settings("Avionix", "Designer");
    settings.remove("windowGeometry");
    settings.remove("windowState");
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
    setSelectionState(index);
    m_viewport->update();
}

void MainWindow::deleteSelectedObject()
{
    const int index = m_viewport->getSelectedIndex();
    if (index < 0)
        return;

    if (!ProjectManager::instance()->removeObject(index))
        return;

    m_objectList->refreshList();
    m_objectList->selectRow(-1);
    m_viewport->setSelectedIndex(-1);
    m_objectProperties->clearProperties();
    setSelectionState(-1);
    m_viewport->update();
}

void MainWindow::alignSelectedObject(int actionId)
{
    const int index = m_viewport->getSelectedIndex();
    if (index < 0)
        return;

    ObjectAlignment alignment = ObjectAlignment::Left;
    switch (actionId) {
    case SelectionToolStrip::AlignTop:
        alignment = ObjectAlignment::Top;
        break;
    case SelectionToolStrip::AlignVCenter:
        alignment = ObjectAlignment::VCenter;
        break;
    case SelectionToolStrip::AlignBottom:
        alignment = ObjectAlignment::Bottom;
        break;
    case SelectionToolStrip::AlignLeft:
        alignment = ObjectAlignment::Left;
        break;
    case SelectionToolStrip::AlignHCenter:
        alignment = ObjectAlignment::HCenter;
        break;
    case SelectionToolStrip::AlignRight:
        alignment = ObjectAlignment::Right;
        break;
    default:
        break;
    }

    if (!ProjectManager::instance()->alignObject(index, alignment))
        return;

    m_objectProperties->showObjectProperties(index);
    m_viewport->update();
}

void MainWindow::setSelectionState(int index)
{
    if (m_selectionToolStrip) {
        m_selectionToolStrip->setSelectionActive(index >= 0);
    }
}

QAction* MainWindow::createAction(const QString &text, const QKeySequence &shortcut, const QObject *receiver, const char *member)
{
    QAction *action = new QAction(text, this);
    action->setShortcut(shortcut);
    connect(action, SIGNAL(triggered()), receiver, member);
    addAction(action);
    return action;
}
