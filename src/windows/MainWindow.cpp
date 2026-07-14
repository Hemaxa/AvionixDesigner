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
#include "TextObject.h"
#include "ViewportPanel.h"
#include "ViewportSettingsPanel.h"

#include <QAction>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QGroupBox>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QSettings>
#include <QSet>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace {
constexpr int kLayoutStateVersion = 2;

QSet<QString> detectAlphabetGroups()
{
    QSet<QString> groups;
    for (const auto &object : ProjectManager::instance()->getObjects()) {
        auto *text = dynamic_cast<TextObject*>(object.data());
        if (!text || !text->isExportEnabled())
            continue;

        for (const QChar ch : text->text) {
            const ushort u = ch.unicode();
            if (ch.isDigit()) {
                groups.insert(QStringLiteral("digits"));
            } else if (u >= 'A' && u <= 'Z') {
                groups.insert(QStringLiteral("latin_upper"));
            } else if (u >= 'a' && u <= 'z') {
                groups.insert(QStringLiteral("latin_lower"));
            } else if ((u >= 0x0410 && u <= 0x042F) || u == 0x0401) {
                groups.insert(QStringLiteral("cyrillic_upper"));
            } else if ((u >= 0x0430 && u <= 0x044F) || u == 0x0451) {
                groups.insert(QStringLiteral("cyrillic_lower"));
            }
        }
    }
    return groups;
}

QIcon makeAlphabetIcon(const QString &text, const QColor &accent)
{
    QPixmap pixmap(34, 26);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(16, 24, 30));
    painter.setPen(QPen(accent, 1));
    painter.drawRoundedRect(QRectF(1, 1, 32, 24), 6, 6);
    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(text.size() > 2 ? 10 : 14);
    painter.setFont(font);
    painter.setPen(QColor(238, 247, 255));
    painter.drawText(pixmap.rect(), Qt::AlignCenter, text);
    painter.end();
    return QIcon(pixmap);
}

QCheckBox* addAlphabetCheck(QVBoxLayout *layout,
                            const QString &title,
                            const QString &key,
                            const QString &iconText,
                            const QColor &iconColor,
                            const QSet<QString> &autoGroups)
{
    auto *check = new QCheckBox(title);
    check->setProperty("alphabetKey", key);
    check->setIcon(makeAlphabetIcon(iconText, iconColor));
    check->setIconSize(QSize(34, 26));
    check->setChecked(autoGroups.contains(key));
    layout->addWidget(check);
    return check;
}

bool collectExportOptions(QWidget *parent, QSet<QString> *alphabetGroups)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Экспорт кадра в XML"));
    dialog.setMinimumSize(560, 360);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto *warning = new QLabel(QStringLiteral(
        "После экспорта изображения и SVG будут растеризованы в маски 0..7. "
        "При повторном открытии экспортированного XML размеры растровых объектов будут заблокированы."
    ), &dialog);
    warning->setWordWrap(true);
    layout->addWidget(warning);

    auto *groupBox = new QGroupBox(QStringLiteral("Алфавиты для добавления в XML"), &dialog);
    auto *groupLayout = new QVBoxLayout(groupBox);
    groupLayout->setSpacing(8);
    const QSet<QString> autoGroups = detectAlphabetGroups();
    QList<QCheckBox*> checks = {
        addAlphabetCheck(groupLayout, QStringLiteral("Цифры 0-9"), QStringLiteral("digits"), QStringLiteral("0-9"), QColor("#56d3ff"), autoGroups),
        addAlphabetCheck(groupLayout, QStringLiteral("Английские заглавные A-Z"), QStringLiteral("latin_upper"), QStringLiteral("A"), QColor("#ffca58"), autoGroups),
        addAlphabetCheck(groupLayout, QStringLiteral("Английские строчные a-z"), QStringLiteral("latin_lower"), QStringLiteral("a"), QColor("#8de1a3"), autoGroups),
        addAlphabetCheck(groupLayout, QStringLiteral("Русские заглавные А-Я, Ё"), QStringLiteral("cyrillic_upper"), QStringLiteral("А"), QColor("#ff7aa2"), autoGroups),
        addAlphabetCheck(groupLayout, QStringLiteral("Русские строчные а-я, ё"), QStringLiteral("cyrillic_lower"), QStringLiteral("я"), QColor("#b89cff"), autoGroups)
    };
    layout->addWidget(groupBox);

    auto *fontNote = new QLabel(QStringLiteral(
        "Для каждого используемого шрифта и размера будет создан отдельный тег font с метриками QFontMetrics и общим data-пулом масок."
    ), &dialog);
    fontNote->setWordWrap(true);
    layout->addWidget(fontNote);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return false;

    alphabetGroups->clear();
    for (QCheckBox *check : checks) {
        if (check->isChecked())
            alphabetGroups->insert(check->property("alphabetKey").toString());
    }
    return true;
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
        "XML (*.xml)"
    );

    if (!fileName.isEmpty()) {
        ProjectManager::instance()->loadFromFile(fileName);
        QSettings settings("Avionix", "Designer");
        settings.setValue("lastProject", fileName);
        m_objectList->refreshList();
        m_objectList->selectRows({});
        m_viewport->setSelectedIndexes({});
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
        project->getFilePath().isEmpty() ? project->getProjectName() + ".xml" : project->getFilePath(),
        "XML (*.xml)"
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
    QSet<QString> alphabetGroups;
    if (!collectExportOptions(this, &alphabetGroups))
        return;

    auto *project = ProjectManager::instance();
    const QString fileName = QFileDialog::getSaveFileName(
        this,
        "Экспорт кадра в XML",
        project->getProjectName() + ".xml",
        "FPGA XML (*.xml)"
    );

    if (!fileName.isEmpty()) {
        project->exportToFpgaXml(fileName, alphabetGroups);
        m_objectList->refreshList();
        m_viewport->update();
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
    fileMenu->addAction(createAction("Экспорт кадра в XML...", QKeySequence("Ctrl+E"), this, SLOT(onExportFpgaXml())));
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
    });
    connect(ProjectManager::instance(), &ProjectManager::projectChanged, m_objectList, &ObjectListPanel::refreshList);
    connect(ProjectManager::instance(), &ProjectManager::projectChanged, m_viewport, QOverload<>::of(&QWidget::update));

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
    connect(m_selectionToolStrip, &SelectionToolStrip::deleteRequested, this, &MainWindow::deleteSelectedObject);
    connect(m_selectionToolStrip, &SelectionToolStrip::exportRequested, this, &MainWindow::onExportFpgaXml);
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

    std::sort(indexes.begin(), indexes.end(), std::greater<int>());
    for (int index : indexes)
        ProjectManager::instance()->removeObject(index);

    m_objectList->refreshList();
    m_objectList->selectRows({});
    m_viewport->setSelectedIndexes({});
    m_objectProperties->clearProperties();
    setSelectionState(false);
    m_viewport->update();
}

void MainWindow::alignSelectedObject(int actionId)
{
    const QList<int> indexes = m_viewport->getSelectedIndexes();
    if (indexes.isEmpty())
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

    double dx = 0.0;
    double dy = 0.0;
    switch (alignment) {
    case ObjectAlignment::Left:
        dx = -bounds.left();
        break;
    case ObjectAlignment::HCenter:
        dx = project->getCanvasWidth() / 2.0 - bounds.center().x();
        break;
    case ObjectAlignment::Right:
        dx = project->getCanvasWidth() - bounds.right();
        break;
    case ObjectAlignment::Top:
        dy = -bounds.top();
        break;
    case ObjectAlignment::VCenter:
        dy = project->getCanvasHeight() / 2.0 - bounds.center().y();
        break;
    case ObjectAlignment::Bottom:
        dy = project->getCanvasHeight() - bounds.bottom();
        break;
    }

    for (int index : indexes) {
        const auto object = project->getObjectAt(index);
        if (object)
            object->moveBy(dx, dy);
    }
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
}

QAction* MainWindow::createAction(const QString &text, const QKeySequence &shortcut, const QObject *receiver, const char *member)
{
    QAction *action = new QAction(text, this);
    action->setShortcut(shortcut);
    connect(action, SIGNAL(triggered()), receiver, member);
    addAction(action);
    return action;
}
