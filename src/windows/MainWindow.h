//MainWindow - главное окно приложения, которое собирает рабочее пространство и служебные панели

#pragma once

#include <QCloseEvent>
#include <QList>
#include <QMainWindow>
#include <QShowEvent>

class QAction;
class QDockWidget;
class EditorWorkspacePanel;
class NewProjectDialog;
class ObjectLibraryPanel;
class ObjectListPanel;
class ObjectPropertiesPanel;
class SelectionToolStrip;
class SettingsWindow;
class ViewportPanel;
class ViewportSettingsPanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

public slots:
    void resetToDefaultLayout();

private slots:
    void onNewProject();
    void onOpenFile();
    void onSaveFile();
    void onSaveFileAs();
    void onExportFpgaXml();
    void onImportImage();
    void updateWindowTitle();
    void openSettings();
    void createObjectOfType(const QString &typeName);
    void createObjectAtPosition(const QString &typeName, const QPointF &pos);
    void deleteSelectedObject();
    void undo();
    void redo();
    void copySelectedObjects();
    void pasteObjects();
    void alignSelectedObject(int actionId);
    void handleSelectionChanged(const QList<int> &indexes);

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void createWidgets();
    void createMenus();
    void connectSignals();
    void setupDockSizes();
    QAction* createAction(const QString &text, const QKeySequence &shortcut, const QObject *receiver, const char *member);
    void saveLayoutSettings();
    void restoreLayoutSettings();
    void setSelectionState(bool active);
    void updateCommandState();

    EditorWorkspacePanel *m_workspacePanel = nullptr;
    ViewportPanel *m_viewport = nullptr;
    SelectionToolStrip *m_selectionToolStrip = nullptr;
    ObjectListPanel *m_objectList = nullptr;
    ObjectPropertiesPanel *m_objectProperties = nullptr;
    ObjectLibraryPanel *m_objectLibrary = nullptr;
    ViewportSettingsPanel *m_viewportSettings = nullptr;

    QDockWidget *m_objectListDock = nullptr;
    QDockWidget *m_objectPropertiesDock = nullptr;
    QDockWidget *m_objectLibraryDock = nullptr;
    QDockWidget *m_viewportSettingsDock = nullptr;

    SettingsWindow *m_settingsWindow = nullptr;
    NewProjectDialog *m_newProjectDialog = nullptr;
    bool m_initialSizesSet = false;
};
