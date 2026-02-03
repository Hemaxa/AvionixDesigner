//WindowManager.h - менеджер окон приложения (синглтон)

#pragma once

#include <QObject>

class ViewportWindow;
class ObjectsListWindow;
class PropertiesWindow;
class LogWindow;

class WindowManager : public QObject
{
    Q_OBJECT

public:
    static WindowManager* instance();

    //создаёт все окна приложения
    void createWindows();
    
    //показывает все окна
    void showAllWindows();
    
    //закрывает все окна
    void closeAllWindows();

    //геттеры окон
    ViewportWindow* viewport() const { return m_viewport; }
    ObjectsListWindow* objectsList() const { return m_objectsList; }
    PropertiesWindow* properties() const { return m_properties; }
    LogWindow* log() const { return m_log; }

public slots:
    void onOpenFile();

private:
    WindowManager();
    ~WindowManager();
    
    void connectSignals();

    ViewportWindow *m_viewport = nullptr;
    ObjectsListWindow *m_objectsList = nullptr;
    PropertiesWindow *m_properties = nullptr;
    LogWindow *m_log = nullptr;
};
