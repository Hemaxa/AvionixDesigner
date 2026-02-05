/**
 * @file PanelsManager.cpp
 * @brief Реализация менеджера панелей
 */

#include "PanelsManager.h"
#include "ProjectManager.h"
#include "../panels/ViewportPanel.h"
#include "../panels/ObjectListPanel.h"
#include "../panels/ObjectPropertiesPanel.h"
#include "../panels/ObjectLibraryPanel.h"
#include "../panels/ViewportSettingsPanel.h"

#include <QFileDialog>

PanelsManager::PanelsManager() {}

PanelsManager::~PanelsManager()
{
    closeAllPanels();
}

PanelsManager* PanelsManager::instance()
{
    static PanelsManager s_instance;
    return &s_instance;
}

void PanelsManager::createPanels()
{
    // Создаём панель холста
    m_viewport = new ViewportPanel();
    
    // Создаём панель списка объектов
    m_objectList = new ObjectListPanel();
    
    // Создаём панель свойств
    m_objectProperties = new ObjectPropertiesPanel();
    
    // Создаём панель библиотеки объектов
    m_objectLibrary = new ObjectLibraryPanel();
    
    // Создаём панель настроек сцены
    m_viewportSettings = new ViewportSettingsPanel();
    
    connectSignals();
}

void PanelsManager::showAllPanels()
{
    if (m_viewport) m_viewport->show();
    if (m_objectList) m_objectList->show();
    if (m_objectProperties) m_objectProperties->show();
    if (m_objectLibrary) m_objectLibrary->show();
    if (m_viewportSettings) m_viewportSettings->show();
}

void PanelsManager::closeAllPanels()
{
    // Удаляем все панели
    if (m_viewport) { delete m_viewport; m_viewport = nullptr; }
    if (m_objectList) { delete m_objectList; m_objectList = nullptr; }
    if (m_objectProperties) { delete m_objectProperties; m_objectProperties = nullptr; }
    if (m_objectLibrary) { delete m_objectLibrary; m_objectLibrary = nullptr; }
    if (m_viewportSettings) { delete m_viewportSettings; m_viewportSettings = nullptr; }
}

void PanelsManager::connectSignals()
{
    // Связь списка объектов с панелью свойств
    connect(m_objectList, &ObjectListPanel::objectSelected,
            m_objectProperties, &ObjectPropertiesPanel::showObjectProperties);
    
    // Обновление списка при загрузке проекта
    connect(ProjectManager::instance(), &ProjectManager::projectLoaded,
            m_objectList, &ObjectListPanel::refreshList);
}

void PanelsManager::onOpenFile()
{
    QString fileName = QFileDialog::getOpenFileName(
        m_viewport,
        "Выберите XML файл",
        QString(),
        "XML Files (*.xml)"
    );
    
    if (!fileName.isEmpty()) {
        ProjectManager::instance()->loadFromFile(fileName);
    }
}
