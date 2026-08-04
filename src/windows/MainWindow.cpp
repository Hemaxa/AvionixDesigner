#include "MainWindow.h"

#include "AppearanceManager.h"
#include "EditorWorkspacePanel.h"
#include "FpgaSchemaRegistry.h"
#include "FontExportDialog.h"
#include "NewProjectDialog.h"
#include "ObjectLibraryPanel.h"
#include "ObjectListPanel.h"
#include "ObjectPropertiesPanel.h"
#include "ProjectManager.h"
#include "SelectionToolStrip.h"
#include "SettingsWindow.h"
#include "TextObject.h"
#include "ViewportPanel.h"
#include "ViewportSettingsPanel.h"
#include "PanelsManager.h"
#include "FpgaStreamingPanel.h"
#include "FpgaSimulatorPanel.h"

#include <QAction>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QIcon>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPointF>
#include <QSettings>
#include <QSet>
#include <QTimer>

#include <algorithm>
#include <functional>

namespace {
constexpr int kLayoutStateVersion = 2;

QString fontExportKey(const QString &family, int pixelSize)
{
    return family + QLatin1Char('\n') + QString::number(pixelSize);
}

QStringList detectedAlphabetGroups(const QString &characters)
{
    QSet<QString> groups;
    for (const QChar ch : characters) {
        const ushort code = ch.unicode();
        if (ch.isDigit()) {
            groups.insert(QStringLiteral("digits"));
        } else if (code >= 'A' && code <= 'Z') {
            groups.insert(QStringLiteral("latin_upper"));
        } else if (code >= 'a' && code <= 'z') {
            groups.insert(QStringLiteral("latin_lower"));
        } else if ((code >= 0x0410 && code <= 0x042F) || code == 0x0401) {
            groups.insert(QStringLiteral("cyrillic_upper"));
        } else if ((code >= 0x0430 && code <= 0x044F) || code == 0x0451) {
            groups.insert(QStringLiteral("cyrillic_lower"));
        } else if (QStringLiteral("α°+-/").contains(ch)) {
            groups.insert(QStringLiteral("symbols"));
        }
    }

    QStringList ordered;
    const QStringList groupOrder = {
        QStringLiteral("digits"),
        QStringLiteral("latin_upper"),
        QStringLiteral("latin_lower"),
        QStringLiteral("cyrillic_upper"),
        QStringLiteral("cyrillic_lower"),
        QStringLiteral("symbols")
    };
    for (const QString &group : groupOrder) {
        if (groups.contains(group))
            ordered.append(group);
    }
    return ordered;
}
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
            if (ProjectManager::instance()->loadFromFile(lastProject))
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
    m_objectList->selectRows({});
    m_objectProperties->clearProperties();
    m_viewport->setSelectedIndexes({});
    m_viewport->resetView();
    setSelectionState(false);
    updateWindowTitle();
}

void MainWindow::onOpenFile()
{
    const QString fileName = QFileDialog::getOpenFileName(
        this,
        "Открыть проект или XML",
        QString(),
        "XML (*.xml *.XML);;Все файлы (*.*)"
    );

    if (!fileName.isEmpty()) {
        auto *project = ProjectManager::instance();
        if (!project->loadFromFile(fileName)) {
            const QString details = project->lastErrorMessage().isEmpty()
                ? tr("Файл не загружен.")
                : project->lastErrorMessage();
            QMessageBox::warning(
                this,
                tr("Не удалось открыть XML"),
                tr("Файл:\n%1\n\n%2").arg(fileName, details)
            );
            return;
        }

        QSettings settings("Avionix", "Designer");
        settings.setValue("lastProject", fileName);
        m_objectList->refreshList();
        m_objectList->selectRows({});
        m_objectProperties->clearProperties();
        m_viewport->setSelectedIndexes({});
        m_viewport->resetView();
        setSelectionState(false);
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

    if (!configureFontExportAlphabets())
        return;

    project->saveToFile();
}

void MainWindow::onSaveFileAs()
{
    auto *project = ProjectManager::instance();

    const QString fileName = QFileDialog::getSaveFileName(
        this,
        "Сохранить проект как...",
        project->getFilePath().isEmpty() ? project->getProjectName() + ".xml" : project->getFilePath(),
        "XML (*.xml)"
    );

    if (!fileName.isEmpty()) {
        if (!configureFontExportAlphabets())
            return;

        project->saveToFile(fileName);
        QSettings settings("Avionix", "Designer");
        settings.setValue("lastProject", fileName);
        updateWindowTitle();
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
    m_objectList->selectRows({index});
    m_viewport->setSelectedIndexes({index});
    m_objectProperties->showObjectProperties(index);
    setSelectionState(true);
    m_viewport->update();
}

void MainWindow::updateWindowTitle()
{
    auto *project = ProjectManager::instance();
    setWindowTitle(QString("Avionix Designer - %1").arg(project->getProjectName()));
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
    m_objectList = PanelsManager::instance()->objectList();
    m_objectProperties = PanelsManager::instance()->objectProperties();
    m_objectLibrary = PanelsManager::instance()->objectLibrary();
    m_viewportSettings = PanelsManager::instance()->viewportSettings();

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

    m_viewportSettingsDock = new QDockWidget("Настройки рабочей области", this);
    m_viewportSettingsDock->setObjectName("ViewportSettingsDock");
    m_viewportSettingsDock->setWidget(m_viewportSettings);
    m_viewportSettingsDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_viewportSettingsDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    m_objectLibraryDock = new QDockWidget("Библиотека объектов", this);
    m_objectLibraryDock->setObjectName("ObjectLibraryDock");
    m_objectLibraryDock->setWidget(m_objectLibrary);
    m_objectLibraryDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_objectLibraryDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    addDockWidget(Qt::RightDockWidgetArea, m_objectListDock);
    addDockWidget(Qt::RightDockWidgetArea, m_objectPropertiesDock);
    splitDockWidget(m_objectListDock, m_objectPropertiesDock, Qt::Vertical);

    m_fpgaStreamingDock = new QDockWidget("Управление ПЛИС", this);
    m_fpgaStreamingDock->setObjectName("FpgaStreamingDock");
    m_fpgaStreamingDock->setWidget(PanelsManager::instance()->fpgaStreaming());
    m_fpgaStreamingDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_fpgaStreamingDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    m_fpgaSimulatorDock = new QDockWidget("Симулятор ПЛИС", this);
    m_fpgaSimulatorDock->setObjectName("FpgaSimulatorDock");
    m_fpgaSimulatorDock->setWidget(PanelsManager::instance()->fpgaSimulator());
    m_fpgaSimulatorDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_fpgaSimulatorDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    addDockWidget(Qt::RightDockWidgetArea, m_fpgaStreamingDock);
    addDockWidget(Qt::RightDockWidgetArea, m_fpgaSimulatorDock);
    tabifyDockWidget(m_objectPropertiesDock, m_fpgaStreamingDock);
    tabifyDockWidget(m_objectListDock, m_fpgaSimulatorDock);

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
    fileMenu->addAction(createAction("Добавить изображение...", QKeySequence("Ctrl+I"), this, SLOT(onImportImage())));
    fileMenu->addSeparator();
    fileMenu->addAction(createAction("Выход", QKeySequence::Quit, this, SLOT(close())));

    QMenu *editMenu = menuBar()->addMenu("Правка");
    editMenu->addAction(createAction("Отменить", QKeySequence::Undo, this, SLOT(undo())));
    editMenu->addAction(createAction("Повторить", QKeySequence::Redo, this, SLOT(redo())));
    editMenu->addSeparator();
    editMenu->addAction(createAction("Копировать", QKeySequence::Copy, this, SLOT(copySelectedObjects())));
    editMenu->addAction(createAction("Вставить", QKeySequence::Paste, this, SLOT(pasteObjects())));
    editMenu->addSeparator();
    editMenu->addAction(createAction("Сгруппировать", QKeySequence("Ctrl+G"), this, SLOT(groupSelectedObjects())));
    editMenu->addAction(createAction("Разгруппировать", QKeySequence("Ctrl+Shift+G"), this, SLOT(ungroupSelectedObjects())));
    editMenu->addSeparator();
    QAction *frontAction = editMenu->addAction("На передний план");
    frontAction->setShortcut(QKeySequence("Ctrl+]"));
    connect(frontAction, &QAction::triggered, this, [this]() {
        alignSelectedObject(SelectionToolStrip::SendToFront);
    });
    addAction(frontAction);

    QAction *backAction = editMenu->addAction("На задний план");
    backAction->setShortcut(QKeySequence("Ctrl+["));
    connect(backAction, &QAction::triggered, this, [this]() {
        alignSelectedObject(SelectionToolStrip::SendToBack);
    });
    addAction(backAction);

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
    viewMenu->addAction(m_fpgaStreamingDock->toggleViewAction());
    viewMenu->addAction(m_fpgaSimulatorDock->toggleViewAction());
    viewMenu->addSeparator();

    QAction *resetViewAction = viewMenu->addAction("Сбросить масштаб");
    resetViewAction->setShortcut(QKeySequence("Ctrl+0"));
    connect(resetViewAction, &QAction::triggered, m_viewport, &ViewportPanel::resetView);

    QAction *deleteAction = new QAction("Удалить объект", this);
    deleteAction->setShortcuts({QKeySequence::Delete, QKeySequence(Qt::Key_Backspace)});
    connect(deleteAction, &QAction::triggered, this, &MainWindow::deleteSelectedObject);
    addAction(deleteAction);
    
    QAction *snapGridAction = viewMenu->addAction("Привязка к сетке (вкл/выкл)");
    snapGridAction->setShortcut(QKeySequence("Shift+G"));
    connect(snapGridAction, &QAction::triggered, this, []() {
        ProjectManager::instance()->setSnapToGrid(!ProjectManager::instance()->snapToGrid());
    });
    
    QAction *snapCanvasAction = viewMenu->addAction("Привязка к экрану (вкл/выкл)");
    snapCanvasAction->setShortcut(QKeySequence("Shift+C"));
    connect(snapCanvasAction, &QAction::triggered, this, []() {
        ProjectManager::instance()->setSnapToCanvas(!ProjectManager::instance()->snapToCanvas());
    });
    
    QAction *snapObjectsAction = viewMenu->addAction("Привязка к объектам (вкл/выкл)");
    snapObjectsAction->setShortcut(QKeySequence("Shift+O"));
    connect(snapObjectsAction, &QAction::triggered, this, []() {
        ProjectManager::instance()->setSnapToObjects(!ProjectManager::instance()->snapToObjects());
    });

    QMenu *settingsMenu = menuBar()->addMenu("Настройки");
    settingsMenu->addAction(createAction("Параметры...", QKeySequence("Ctrl+,"), this, SLOT(openSettings())));
}

void MainWindow::connectSignals()
{
    connect(m_objectList, &ObjectListPanel::selectionChanged, m_viewport, &ViewportPanel::setSelectedIndexes);
    connect(m_objectList, &ObjectListPanel::selectionChanged, this, &MainWindow::handleSelectionChanged);

    connect(m_viewport, &ViewportPanel::selectionChanged, m_objectList, &ObjectListPanel::selectRows);
    connect(m_viewport, &ViewportPanel::selectionChanged, this, &MainWindow::handleSelectionChanged);

    connect(ProjectManager::instance(), &ProjectManager::projectLoaded, this, &MainWindow::updateWindowTitle);
    connect(ProjectManager::instance(), &ProjectManager::projectLoaded, m_objectProperties, &ObjectPropertiesPanel::clearProperties);
    connect(ProjectManager::instance(), &ProjectManager::projectLoaded, this, [this]() {
        m_viewport->setSelectedIndexes({});
        m_objectList->selectRows({});
        setSelectionState(false);
        updateCommandState();
    });
    connect(ProjectManager::instance(), &ProjectManager::projectChanged, m_objectList, &ObjectListPanel::refreshList);
    connect(ProjectManager::instance(), &ProjectManager::projectChanged, m_viewport, QOverload<>::of(&QWidget::update));
    connect(ProjectManager::instance(), &ProjectManager::projectChanged, this, &MainWindow::updateCommandState);

    connect(m_objectProperties, &ObjectPropertiesPanel::propertyChanged, m_viewport, QOverload<>::of(&QWidget::update));
    connect(m_viewport, &ViewportPanel::objectChanged, this, [this]() {
        const QList<int> indexes = m_viewport->getSelectedIndexes();
        if (indexes.size() == 1)
            m_objectProperties->showObjectProperties(indexes.first());
        else
            m_objectProperties->clearProperties();
    });

    connect(m_objectLibrary, &ObjectLibraryPanel::objectRequested, this, &MainWindow::createObjectOfType);
    connect(m_objectLibrary, &ObjectLibraryPanel::imageImportRequested, this, &MainWindow::onImportImage);
    connect(m_selectionToolStrip, &SelectionToolStrip::alignRequested, this, &MainWindow::alignSelectedObject);
    connect(m_selectionToolStrip, &SelectionToolStrip::undoRequested, this, &MainWindow::undo);
    connect(m_selectionToolStrip, &SelectionToolStrip::redoRequested, this, &MainWindow::redo);
    connect(m_selectionToolStrip, &SelectionToolStrip::copyRequested, this, &MainWindow::copySelectedObjects);
    connect(m_selectionToolStrip, &SelectionToolStrip::pasteRequested, this, &MainWindow::pasteObjects);
    connect(m_selectionToolStrip, &SelectionToolStrip::deleteRequested, this, &MainWindow::deleteSelectedObject);
    connect(m_selectionToolStrip, &SelectionToolStrip::groupRequested, this, &MainWindow::groupSelectedObjects);
    connect(m_selectionToolStrip, &SelectionToolStrip::ungroupRequested, this, &MainWindow::ungroupSelectedObjects);
    connect(m_viewport, &ViewportPanel::imageDropped, this, [this](const QString &fileName) {
        const int index = ProjectManager::instance()->importImageAsStaticGroup(fileName);
        if (index < 0)
            return;

        m_objectList->refreshList();
        m_objectList->selectRows({index});
        m_viewport->setSelectedIndexes({index});
        m_objectProperties->showObjectProperties(index);
        setSelectionState(true);
        m_viewport->update();
    });

    connect(m_viewport, &ViewportPanel::objectDropped, this, &MainWindow::createObjectAtPosition);
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
    m_objectList->selectRows({index});
    m_viewport->setSelectedIndexes({index});
    m_objectProperties->showObjectProperties(index);
    setSelectionState(true);
    m_viewport->update();
}

void MainWindow::createObjectAtPosition(const QString &typeName, const QPointF &pos)
{
    const int index = ProjectManager::instance()->addObject(typeName, pos.x(), pos.y());
    if (index < 0)
        return;

    m_objectList->refreshList();
    m_objectList->selectRows({index});
    m_viewport->setSelectedIndexes({index});
    m_objectProperties->showObjectProperties(index);
    setSelectionState(true);
    m_viewport->update();
}

void MainWindow::deleteSelectedObject()
{
    QList<int> indexes = m_viewport->getSelectedIndexes();
    if (indexes.isEmpty())
        return;

    if (!ProjectManager::instance()->removeObjects(indexes))
        return;

    m_objectList->refreshList();
    m_objectList->selectRows({});
    m_viewport->setSelectedIndexes({});
    m_objectProperties->clearProperties();
    setSelectionState(false);
    m_viewport->update();
}

void MainWindow::undo()
{
    if (!ProjectManager::instance()->undo())
        return;

    m_objectList->selectRows({});
    m_viewport->setSelectedIndexes({});
    m_objectProperties->clearProperties();
    setSelectionState(false);
    updateCommandState();
}

void MainWindow::redo()
{
    if (!ProjectManager::instance()->redo())
        return;

    m_objectList->selectRows({});
    m_viewport->setSelectedIndexes({});
    m_objectProperties->clearProperties();
    setSelectionState(false);
    updateCommandState();
}

void MainWindow::copySelectedObjects()
{
    const QList<int> indexes = m_viewport->getSelectedIndexes();
    if (indexes.isEmpty())
        return;

    ProjectManager::instance()->copyObjects(indexes);
    updateCommandState();
}

void MainWindow::pasteObjects()
{
    const QList<int> pastedIndexes = ProjectManager::instance()->pasteObjects();
    if (pastedIndexes.isEmpty())
        return;

    m_objectList->refreshList();
    m_objectList->selectRows(pastedIndexes);
    m_viewport->setSelectedIndexes(pastedIndexes);
    handleSelectionChanged(pastedIndexes);
    m_viewport->update();
    updateCommandState();
}

bool MainWindow::configureFontExportAlphabets()
{
    auto *project = ProjectManager::instance();
    QMap<QString, FontExportDialogEntry> entriesByKey;
    QMap<QString, QList<TextObject*>> textsByKey;

    for (const auto &object : project->getObjects()) {
        auto *text = dynamic_cast<TextObject*>(object.data());
        if (!text || !text->isExportEnabled() || text->hasFontAtlas())
            continue;

        const QString key = fontExportKey(text->fontFamily, text->pixelSize);
        FontExportDialogEntry entry = entriesByKey.value(key);
        if (entry.fontFamily.isEmpty()) {
            entry.fontFamily = text->fontFamily;
            entry.pixelSize = text->pixelSize;
        }

        entry.textCount += 1;
        if (!entry.sampleText.contains(text->text)) {
            if (!entry.sampleText.isEmpty())
                entry.sampleText.append(QStringLiteral("; "));
            entry.sampleText.append(text->text);
        }

        const QStringList detectedGroups = detectedAlphabetGroups(text->exportCharacters());
        for (const QString &group : detectedGroups) {
            if (!entry.alphabetGroups.contains(group))
                entry.alphabetGroups.append(group);
        }

        entriesByKey.insert(key, entry);
        textsByKey[key].append(text);
    }

    if (entriesByKey.isEmpty())
        return true;

    QList<FontExportDialogEntry> entries;
    for (auto it = entriesByKey.constBegin(); it != entriesByKey.constEnd(); ++it)
        entries.append(it.value());

    FontExportDialog dialog(entries, this);
    if (dialog.exec() != QDialog::Accepted)
        return false;

    for (const FontExportDialogEntry &entry : dialog.entries()) {
        const QString key = fontExportKey(entry.fontFamily, entry.pixelSize);
        for (TextObject *text : textsByKey.value(key))
            text->exportAlphabetGroups = entry.alphabetGroups;
    }

    return true;
}

void MainWindow::groupSelectedObjects()
{
    const QList<int> indexes = m_viewport->getSelectedIndexes();
    if (indexes.size() < 2)
        return;

    auto *project = ProjectManager::instance();
    const int groupId = project->groupObjects(indexes);
    if (groupId < 0)
        return;

    const QList<int> groupMembers = project->groupMembers(groupId);
    m_objectList->refreshList();
    m_objectList->selectRows(groupMembers);
    m_viewport->setSelectedIndexes(groupMembers);
    handleSelectionChanged(groupMembers);
    m_viewport->update();
}

void MainWindow::ungroupSelectedObjects()
{
    const QList<int> indexes = m_viewport->getSelectedIndexes();
    if (indexes.isEmpty())
        return;

    if (!ProjectManager::instance()->ungroupObjects(indexes))
        return;

    m_objectList->refreshList();
    m_objectList->selectRows(indexes);
    m_viewport->setSelectedIndexes(indexes);
    handleSelectionChanged(indexes);
    m_viewport->update();
}

void MainWindow::alignSelectedObject(int actionId)
{
    const QList<int> indexes = m_viewport->getSelectedIndexes();
    if (indexes.isEmpty())
        return;

    if (actionId == SelectionToolStrip::SendToFront || actionId == SelectionToolStrip::SendToBack) {
        auto *project = ProjectManager::instance();
        const bool ok = actionId == SelectionToolStrip::SendToFront
            ? project->sendObjectsToFront(indexes)
            : project->sendObjectsToBack(indexes);
        if (!ok)
            return;

        QList<int> newIndexes;
        if (actionId == SelectionToolStrip::SendToFront) {
            for (int i = 0; i < indexes.size(); ++i)
                newIndexes.append(i);
        } else {
            const int first = project->getObjectCount() - indexes.size();
            for (int i = 0; i < indexes.size(); ++i)
                newIndexes.append(first + i);
        }
        m_objectList->refreshList();
        m_objectList->selectRows(newIndexes);
        m_viewport->setSelectedIndexes(newIndexes);
        handleSelectionChanged(newIndexes);
        return;
    }

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

    if (indexes.size() == 1) {
        const int index = indexes.first();
        if (!ProjectManager::instance()->alignObject(index, alignment))
            return;
        m_objectProperties->showObjectProperties(index);
        m_viewport->update();
        return;
    }

    QRectF bounds;
    bool hasBounds = false;
    auto *project = ProjectManager::instance();
    for (int index : indexes) {
        const auto object = project->getObjectAt(index);
        if (!object)
            continue;
        bounds = hasBounds ? bounds.united(object->getBoundingRect()) : object->getBoundingRect();
        hasBounds = true;
    }

    if (!hasBounds || bounds.isEmpty())
        return;

    struct MoveDelta
    {
        int index = -1;
        QPointF delta;
    };

    QList<MoveDelta> deltas;
    for (int index : indexes) {
        const auto object = project->getObjectAt(index);
        if (!object)
            continue;

        const QRectF objectBounds = object->getBoundingRect();
        QPointF delta;
        switch (alignment) {
        case ObjectAlignment::Left:
            delta.setX(bounds.left() - objectBounds.left());
            break;
        case ObjectAlignment::HCenter:
            delta.setX(bounds.center().x() - objectBounds.center().x());
            break;
        case ObjectAlignment::Right:
            delta.setX(bounds.right() - objectBounds.right());
            break;
        case ObjectAlignment::Top:
            delta.setY(bounds.top() - objectBounds.top());
            break;
        case ObjectAlignment::VCenter:
            delta.setY(bounds.center().y() - objectBounds.center().y());
            break;
        case ObjectAlignment::Bottom:
            delta.setY(bounds.bottom() - objectBounds.bottom());
            break;
        }

        if (!qFuzzyIsNull(delta.x()) || !qFuzzyIsNull(delta.y()))
            deltas.append({index, delta});
    }

    if (deltas.isEmpty())
        return;

    project->recordObjectEdit();
    for (const MoveDelta &move : deltas) {
        const auto object = project->getObjectAt(move.index);
        if (object)
            object->moveBy(move.delta.x(), move.delta.y());
    }
    project->finishObjectEdit(tr("Выполнено выравнивание объектов"));
    m_objectProperties->clearProperties();
    m_viewport->update();
}

void MainWindow::handleSelectionChanged(const QList<int> &indexes)
{
    if (indexes.size() == 1)
        m_objectProperties->showObjectProperties(indexes.first());
    else
        m_objectProperties->clearProperties();

    setSelectionState(!indexes.isEmpty());
}

void MainWindow::setSelectionState(bool active)
{
    if (m_selectionToolStrip) {
        m_selectionToolStrip->setSelectionActive(active);
    }
    updateCommandState();
}

void MainWindow::updateCommandState()
{
    if (!m_selectionToolStrip)
        return;

    auto *project = ProjectManager::instance();
    m_selectionToolStrip->setHistoryAvailable(project->canUndo(), project->canRedo());
    m_selectionToolStrip->setPasteAvailable(project->canPasteObjects());
}

QAction* MainWindow::createAction(const QString &text, const QKeySequence &shortcut, const QObject *receiver, const char *member)
{
    QAction *action = new QAction(text, this);
    action->setShortcut(shortcut);
    connect(action, SIGNAL(triggered()), receiver, member);
    addAction(action);
    return action;
}
