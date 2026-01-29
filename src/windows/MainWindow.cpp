/**
 * @file MainWindow.cpp
 * @brief Реализация главного окна
 */

#include "MainWindow.h"
#include "ViewportWindow.h"
#include "ObjectsListWindow.h"
#include "PropertiesWindow.h"
#include "LogWindow.h"
#include "../managers/ProjectManager.h"
#include "../managers/AppearanceManager.h"
#include <QDockWidget>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("XML Editor");
    
    AppearanceManager::instance()->applyDarkTheme();
    
    createWidgets();
    createMenus();
    connectSignals();
    
    showMaximized();
    
    QTimer::singleShot(100, this, &MainWindow::onOpenFile);
}

void MainWindow::onOpenFile()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Выберите XML файл",
        QString(),
        "XML Files (*.xml)"
    );
    
    if (!fileName.isEmpty()) {
        ProjectManager::instance()->loadFromFile(fileName);
        updateWindowTitle();
    }
}

void MainWindow::updateWindowTitle()
{
    auto pm = ProjectManager::instance();
    setWindowTitle(QString("XML Editor - %1").arg(pm->getProjectName()));
}

void MainWindow::createWidgets()
{
    // Центральный виджет - холст
    m_viewport = new ViewportWindow(this);
    setCentralWidget(m_viewport);
    
    // Панель списка объектов (слева)
    m_objectsList = new ObjectsListWindow(this);
    m_objectsListDock = new QDockWidget("Объекты", this);
    m_objectsListDock->setWidget(m_objectsList);
    m_objectsListDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, m_objectsListDock);
    
    // Панель свойств (справа)
    m_properties = new PropertiesWindow(this);
    m_propertiesDock = new QDockWidget("Свойства", this);
    m_propertiesDock->setWidget(m_properties);
    m_propertiesDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);
    
    // Панель лога (снизу)
    m_log = new LogWindow(this);
    m_logDock = new QDockWidget("Лог", this);
    m_logDock->setWidget(m_log);
    m_logDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu("Файл");
    
    QAction *openAction = fileMenu->addAction("Открыть...");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenFile);
    
    fileMenu->addSeparator();
    
    QAction *exitAction = fileMenu->addAction("Выход");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QMainWindow::close);
    
    QMenu *viewMenu = menuBar()->addMenu("Вид");
    viewMenu->addAction(m_objectsListDock->toggleViewAction());
    viewMenu->addAction(m_propertiesDock->toggleViewAction());
    viewMenu->addAction(m_logDock->toggleViewAction());
    
    viewMenu->addSeparator();
    
    QAction *resetViewAction = viewMenu->addAction("Сбросить вид");
    resetViewAction->setShortcut(QKeySequence("Ctrl+0"));
    connect(resetViewAction, &QAction::triggered, m_viewport, &ViewportWindow::resetView);
}

void MainWindow::connectSignals()
{
    connect(m_objectsList, &ObjectsListWindow::objectSelected,
            m_properties, &PropertiesWindow::showObjectProperties);
    
    connect(ProjectManager::instance(), &ProjectManager::projectLoaded,
            this, &MainWindow::updateWindowTitle);
}
