/**
 * @file MainWindow.h
 * @brief Главное окно приложения
 */

#pragma once

#include <QMainWindow>

class QDockWidget;
class ViewportWindow;
class ObjectsListWindow;
class PropertiesWindow;
class LogWindow;

/**
 * @class MainWindow
 * @brief Главное окно приложения с dock-панелями
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onOpenFile();
    void updateWindowTitle();

private:
    void createWidgets();
    void createMenus();
    void connectSignals();
    
    ViewportWindow *m_viewport;
    ObjectsListWindow *m_objectsList;
    PropertiesWindow *m_properties;
    LogWindow *m_log;
    
    QDockWidget *m_objectsListDock;
    QDockWidget *m_propertiesDock;
    QDockWidget *m_logDock;
};
