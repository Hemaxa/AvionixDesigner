#include "PanelsManager.h"
#include "ProjectManager.h"
#include "ViewportPanel.h"
#include "ObjectListPanel.h"
#include "ObjectPropertiesPanel.h"
#include "ObjectLibraryPanel.h"
#include "ViewportSettingsPanel.h"

#include <QFileDialog>
#include <QMessageBox>

PanelsManager::PanelsManager() = default;

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
    m_viewport = new ViewportPanel();
    m_objectList = new ObjectListPanel();
    m_objectProperties = new ObjectPropertiesPanel();
    m_objectLibrary = new ObjectLibraryPanel();
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
    if (m_viewport) { delete m_viewport; m_viewport = nullptr; }
    if (m_objectList) { delete m_objectList; m_objectList = nullptr; }
    if (m_objectProperties) { delete m_objectProperties; m_objectProperties = nullptr; }
    if (m_objectLibrary) { delete m_objectLibrary; m_objectLibrary = nullptr; }
    if (m_viewportSettings) { delete m_viewportSettings; m_viewportSettings = nullptr; }
}

void PanelsManager::connectSignals()
{
    // Список -> свойства
    connect(m_objectList, &ObjectListPanel::objectSelected, m_objectProperties, &ObjectPropertiesPanel::showObjectProperties);
    // Холст -> список
    connect(m_viewport, &ViewportPanel::objectSelected, m_objectList, [this](int index) {
        m_objectList->selectRows(index >= 0 ? QList<int>{index} : QList<int>{});
    });
    // Изменение на холсте -> свойства
    connect(m_viewport, &ViewportPanel::objectChanged, [this]() {
        // Перепоказать свойства для текущего выбранного объекта.
        if (m_objectList) {
            m_objectProperties->showObjectProperties(m_viewport->getSelectedIndex());
        }
    });
    
    connect(ProjectManager::instance(), &ProjectManager::projectLoaded, m_objectList, &ObjectListPanel::refreshList);
}

void PanelsManager::onOpenFile()
{
    QString fileName = QFileDialog::getOpenFileName(
        m_viewport,
        "Выберите XML файл",
        QString(),
        "XML (*.xml *.XML);;Все файлы (*.*)"
    );
    
    if (!fileName.isEmpty()) {
        auto *project = ProjectManager::instance();
        if (!project->loadFromFile(fileName)) {
            const QString details = project->lastErrorMessage().isEmpty()
                ? QStringLiteral("Файл не загружен.")
                : project->lastErrorMessage();
            QMessageBox::warning(
                m_viewport,
                QStringLiteral("Не удалось открыть XML"),
                QStringLiteral("Файл:\n%1\n\n%2").arg(fileName, details)
            );
        }
    }
}
