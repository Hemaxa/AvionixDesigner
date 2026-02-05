/**
 * @file PanelsManager.h
 * @brief Менеджер панелей приложения (синглтон)
 */

#pragma once

#include <QObject>

class ViewportPanel;
class ObjectListPanel;
class ObjectPropertiesPanel;
class ObjectLibraryPanel;
class ViewportSettingsPanel;

/**
 * @class PanelsManager
 * @brief Менеджер панелей приложения
 */
class PanelsManager : public QObject
{
    Q_OBJECT

public:
    // Получение единственного экземпляра
    static PanelsManager* instance();

    // Создаёт все панели приложения
    void createPanels();
    
    // Показывает все панели
    void showAllPanels();
    
    // Закрывает все панели
    void closeAllPanels();

    // Геттеры панелей
    ViewportPanel* viewport() const { return m_viewport; }
    ObjectListPanel* objectList() const { return m_objectList; }
    ObjectPropertiesPanel* objectProperties() const { return m_objectProperties; }
    ObjectLibraryPanel* objectLibrary() const { return m_objectLibrary; }
    ViewportSettingsPanel* viewportSettings() const { return m_viewportSettings; }

public slots:
    // Обработчик открытия файла
    void onOpenFile();

private:
    PanelsManager();
    ~PanelsManager();
    
    // Соединяет сигналы между панелями
    void connectSignals();

    ViewportPanel *m_viewport = nullptr;
    ObjectListPanel *m_objectList = nullptr;
    ObjectPropertiesPanel *m_objectProperties = nullptr;
    ObjectLibraryPanel *m_objectLibrary = nullptr;
    ViewportSettingsPanel *m_viewportSettings = nullptr;
};
