//WindowManager.cpp - реализация менеджера окон

#include "WindowManager.h"
#include "ProjectManager.h"
#include "AppearanceManager.h"
#include "../windows/ViewportWindow.h"
#include "../windows/ObjectsListWindow.h"
#include "../windows/PropertiesWindow.h"
#include "../windows/LogWindow.h"

#include <QFileDialog>
#include <QApplication>
#include <QScreen>

WindowManager::WindowManager() {}

WindowManager::~WindowManager()
{
    closeAll();
}

WindowManager* WindowManager::instance()
{
    static WindowManager s_instance;
    return &s_instance;
}

void WindowManager::createWindows()
{
    //получаем геометрию экрана для расположения окон
    QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();
    
    int screenWidth = screenGeometry.width();
    int screenHeight = screenGeometry.height();
    
    //создаём окно холста (основное, самое большое)
    m_viewport = new ViewportWindow();
    m_viewport->setWindowTitle("Холст");
    m_viewport->setGeometry(
        screenGeometry.x() + 250,           //отступ слева для панели объектов
        screenGeometry.y(),
        screenWidth - 550,                   //ширина с учётом боковых панелей
        screenHeight - 200                   //высота с учётом лога
    );
    
    //создаём окно списка объектов (слева)
    m_objectsList = new ObjectsListWindow();
    m_objectsList->setWindowTitle("Объекты");
    m_objectsList->setGeometry(
        screenGeometry.x(),
        screenGeometry.y(),
        240,
        screenHeight - 200
    );
    
    //создаём окно свойств (справа)
    m_properties = new PropertiesWindow();
    m_properties->setWindowTitle("Свойства");
    m_properties->setGeometry(
        screenGeometry.x() + screenWidth - 300,
        screenGeometry.y(),
        290,
        screenHeight - 200
    );
    
    //создаём окно лога (снизу)
    m_log = new LogWindow();
    m_log->setWindowTitle("Лог");
    m_log->setGeometry(
        screenGeometry.x(),
        screenGeometry.y() + screenHeight - 190,
        screenWidth,
        180
    );
    
    connectSignals();
}

void WindowManager::showAllWindows()
{
    if (m_viewport) m_viewport->show();
    if (m_objectsList) m_objectsList->show();
    if (m_properties) m_properties->show();
    if (m_log) m_log->show();
    
    //активируем окно холста
    if (m_viewport) m_viewport->raise();
}

void WindowManager::closeAllWindows()
{
    if (m_viewport) { m_viewport->close(); delete m_viewport; m_viewport = nullptr; }
    if (m_objectsList) { m_objectsList->close(); delete m_objectsList; m_objectsList = nullptr; }
    if (m_properties) { m_properties->close(); delete m_properties; m_properties = nullptr; }
    if (m_log) { m_log->close(); delete m_log; m_log = nullptr; }
}

void WindowManager::connectSignals()
{
    //связь списка объектов с панелью свойств
    connect(m_objectsList, &ObjectsListWindow::objectSelected,
            m_properties, &PropertiesWindow::showObjectProperties);
    
    //связь ProjectManager с логом
    connect(ProjectManager::instance(), &ProjectManager::logMessage,
            m_log, &LogWindow::log);
    
    //обновление списка при загрузке проекта
    connect(ProjectManager::instance(), &ProjectManager::projectLoaded,
            m_objectsList, &ObjectsListWindow::refreshList);
    
    //обновление холста при загрузке проекта
    connect(ProjectManager::instance(), &ProjectManager::projectLoaded,
            m_viewport, QOverload<>::of(&QWidget::update));
}

void WindowManager::onOpenFile()
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
