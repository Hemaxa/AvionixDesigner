/**
 * @file MainWindow.h
 * @brief Главное окно приложения - сборщик панелей
 */

#pragma once

#include <QMainWindow>

class QDockWidget;
class QSplitter;
class ViewportPanel;
class ObjectListPanel;
class ObjectPropertiesPanel;
class ObjectLibraryPanel;
class ViewportSettingsPanel;
class SettingsWindow;

/**
 * @class MainWindow
 * @brief Главное окно приложения, собирающее все панели
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    // Открыть файл проекта
    void onOpenFile();
    
    // Обновить заголовок окна
    void updateWindowTitle();
    
    // Открыть окно настроек
    void openSettings();

private:
    // Создание виджетов и панелей
    void createWidgets();
    
    // Создание меню
    void createMenus();
    
    // Соединение сигналов
    void connectSignals();
    
    // Панели
    ViewportPanel *m_viewport;
    ObjectListPanel *m_objectList;
    ObjectPropertiesPanel *m_objectProperties;
    ObjectLibraryPanel *m_objectLibrary;
    ViewportSettingsPanel *m_viewportSettings;
    
    // Dock-панели
    QDockWidget *m_objectListDock;
    QDockWidget *m_objectPropertiesDock;
    QDockWidget *m_bottomDock;
    
    // Окно настроек
    SettingsWindow *m_settingsWindow;
};
