//PanelsManager - менеджер панелей приложения

#pragma once

#include <QObject>

class ViewportPanel;
class ObjectListPanel;
class ObjectPropertiesPanel;
class ObjectLibraryPanel;
class ViewportSettingsPanel;
class FpgaStreamingPanel;
class FpgaSimulatorPanel;

class PanelsManager : public QObject
{
    Q_OBJECT

public:
    //получение единственного экземпляра
    static PanelsManager* instance();

    //создаёт все панели приложения
    void createPanels();
    
    //показывает все панели
    void showAllPanels();
    
    //закрывает все панели
    void closeAllPanels();

    //геттеры панелей
    ViewportPanel* viewport() const { return m_viewport; }
    ObjectListPanel* objectList() const { return m_objectList; }
    ObjectPropertiesPanel* objectProperties() const { return m_objectProperties; }
    ObjectLibraryPanel* objectLibrary() const { return m_objectLibrary; }
    ViewportSettingsPanel* viewportSettings() const { return m_viewportSettings; }
    FpgaStreamingPanel* fpgaStreaming() const { return m_fpgaStreaming; }
    FpgaSimulatorPanel* fpgaSimulator() const { return m_fpgaSimulator; }

public slots:
    //обработчик открытия файла
    void onOpenFile();

private:
    PanelsManager();
    ~PanelsManager();
    
    //соединяет сигналы между панелями
    void connectSignals();

    ViewportPanel *m_viewport = nullptr;
    ObjectListPanel *m_objectList = nullptr;
    ObjectPropertiesPanel *m_objectProperties = nullptr;
    ObjectLibraryPanel *m_objectLibrary = nullptr;
    ViewportSettingsPanel *m_viewportSettings = nullptr;
    FpgaStreamingPanel *m_fpgaStreaming = nullptr;
    FpgaSimulatorPanel *m_fpgaSimulator = nullptr;
};
