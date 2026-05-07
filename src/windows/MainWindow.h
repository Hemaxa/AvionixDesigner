//MainWindow - главное окно приложения, которое является сборщиком панелей

#pragma once //директива для единоразового включения заголовочного файла

#include <QMainWindow>
#include <QShowEvent>
#include <QCloseEvent>

//предварительное объявление классов (forward declaration)
class QDockWidget;
class ViewportPanel;
class ObjectListPanel;
class ObjectPropertiesPanel;
class ObjectLibraryPanel;
class ViewportSettingsPanel;
class SettingsWindow;

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    //конструктор с запретом на неявное преобразование типов
    explicit MainWindow(QWidget *parent = nullptr);

public slots:
    //сброс layout до стандартных значений
    void resetToDefaultLayout();

private slots:
    //слот, который вызывается при открытии файла проекта
    void onOpenFile();
    
    //слот, который вызывается при сохранении текущего проекта
    void onSaveFile();
    
    //слот, который вызывается при сохранении проекта в новый файл
    void onSaveFileAs();
    
    //слот, который вызывается при обновлении заголовока окна
    void updateWindowTitle();
    
    //слот, который вызывается при открытии окна настроек
    void openSettings();

protected:
    //переопределение события показа окна для применения размеров
    void showEvent(QShowEvent *event) override;
    
    //переопределение события закрытия для сохранения layout
    void closeEvent(QCloseEvent *event) override;

private:
    //метод создания виджетов и панелей
    void createWidgets();
    
    //метод создания меню
    void createMenus();
    
    //метод соединения сигналов
    void connectSignals();
    
    //метод настройки размеров dock-виджетов
    void setupDockSizes();
    
    //сохранение и восстановление layout
    void saveLayoutSettings();
    void restoreLayoutSettings();
    
    //указатели на панели
    ViewportPanel *m_viewport;
    ObjectListPanel *m_objectList;
    ObjectPropertiesPanel *m_objectProperties;
    ObjectLibraryPanel *m_objectLibrary;
    ViewportSettingsPanel *m_viewportSettings;
    
    //dock-виджеты для каждой панели (перетаскиваемое окно)
    QDockWidget *m_viewportDock;
    QDockWidget *m_objectListDock;
    QDockWidget *m_objectPropertiesDock;
    QDockWidget *m_objectLibraryDock;
    QDockWidget *m_viewportSettingsDock;
    
    //окно настроек
    SettingsWindow *m_settingsWindow;
    
    //флаг первоначальной настройки размеров
    bool m_initialSizesSet = false;
};
