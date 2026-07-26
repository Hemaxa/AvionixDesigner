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
    //сбрасывает расположение dock-панелей к стандартному виду
    void resetToDefaultLayout();

private slots:
    //создаёт новый проект через диалог параметров
    void onNewProject();
    //открывает XML-файл проекта
    void onOpenFile();
    //сохраняет текущий проект
    void onSaveFile();
    //сохраняет текущий проект по новому пути
    void onSaveFileAs();
    //импортирует изображение как объект static_group/image
    void onImportImage();
    //обновляет заголовок окна по текущему проекту
    void updateWindowTitle();
    //открывает окно настроек приложения
    void openSettings();
    //создаёт объект выбранного типа в стандартной позиции
    void createObjectOfType(const QString &typeName);
    //создаёт объект выбранного типа в позиции на холсте
    void createObjectAtPosition(const QString &typeName, const QPointF &pos);
    //удаляет выбранные объекты
    void deleteSelectedObject();
    //отменяет последнее действие
    void undo();
    //повторяет отменённое действие
    void redo();
    //копирует выбранные объекты
    void copySelectedObjects();
    //вставляет объекты из внутреннего буфера
    void pasteObjects();
    //выравнивает выбранные объекты или меняет их порядок слоёв
    void alignSelectedObject(int actionId);
    //создаёт редакторскую группу из выбранных объектов
    void groupSelectedObjects();
    //разгруппировывает выбранную редакторскую группу
    void ungroupSelectedObjects();
    //синхронизирует панели со сменой выделения
    void handleSelectionChanged(const QList<int> &indexes);

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    //создаёт центральные и dock-виджеты главного окна
    void createWidgets();
    //создаёт пункты главного меню
    void createMenus();
    //соединяет сигналы между панелями и менеджером проекта
    void connectSignals();
    //задаёт начальные размеры dock-панелей
    void setupDockSizes();
    //создаёт QAction с текстом, shortcut и receiver/member
    QAction* createAction(const QString &text, const QKeySequence &shortcut, const QObject *receiver, const char *member);
    //сохраняет геометрию и состояние dock-панелей
    void saveLayoutSettings();
    //восстанавливает геометрию и состояние dock-панелей
    void restoreLayoutSettings();
    //обновляет доступность команд, зависящих от выделения
    void setSelectionState(bool active);
    //обновляет доступность команд истории и вставки
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
