#include "mainwindow.h"
#include <QFile>
#include <QDomDocument>
#include <QFileDialog>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QScreen>
#include <QApplication>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_projectWidth(640)
    , m_projectHeight(480)
    , m_bgColor(Qt::black)
{
    setupUI();
    setupMenus();
    
    // Разворачиваем на весь экран
    showMaximized();
    
    // Показываем диалог выбора файла при запуске
    QTimer::singleShot(100, this, &MainWindow::onOpenFile);
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI()
{
    setWindowTitle("XML Editor");
    
    // Создаем центральный виджет (Canvas)
    m_canvas = new CanvasWidget(this);
    setCentralWidget(m_canvas);
    
    // Создаем панель списка объектов (слева)
    m_objectList = new ObjectListWidget(this);
    m_objectListDock = new QDockWidget("Объекты", this);
    m_objectListDock->setWidget(m_objectList);
    m_objectListDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, m_objectListDock);
    
    // Создаем панель свойств (справа)
    m_properties = new PropertiesWidget(this);
    m_propertiesDock = new QDockWidget("Свойства", this);
    m_propertiesDock->setWidget(m_properties);
    m_propertiesDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);
    
    // Создаем панель лога (снизу)
    m_log = new LogWidget(this);
    m_logDock = new QDockWidget("Лог", this);
    m_logDock->setWidget(m_log);
    m_logDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);
    
    // Подключаем сигналы
    connect(m_objectList, &ObjectListWidget::objectSelected, this, &MainWindow::onObjectSelected);
    
    log("Приложение запущено");
}

void MainWindow::setupMenus()
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
    viewMenu->addAction(m_objectListDock->toggleViewAction());
    viewMenu->addAction(m_propertiesDock->toggleViewAction());
    viewMenu->addAction(m_logDock->toggleViewAction());
}

void MainWindow::onOpenFile()
{
    QString fileName = QFileDialog::getOpenFileName(this, 
        "Выберите XML файл", 
        QString(), 
        "XML Files (*.xml)");
    
    if (!fileName.isEmpty()) {
        loadXml(fileName);
    }
}

void MainWindow::onObjectSelected(int index)
{
    if (index >= 0 && index < m_shapes.size()) {
        m_properties->showProperties(m_shapes[index]);
        log(QString("Выбран объект #%1").arg(index + 1));
    }
}

void MainWindow::log(const QString &msg)
{
    m_log->log(msg);
}

bool MainWindow::loadXml(const QString &fileName)
{
    log("Загрузка: " + fileName);

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        log("ОШИБКА: Не удалось открыть файл");
        return false;
    }

    QDomDocument doc;
    QString errorMsg;
    int errorLine;
    if (!doc.setContent(&file, false, &errorMsg, &errorLine)) {
        log(QString("ОШИБКА XML: %1 (строка %2)").arg(errorMsg).arg(errorLine));
        file.close();
        return false;
    }
    file.close();

    QDomElement root = doc.documentElement();
    
    // Читаем параметры проекта
    m_projectWidth = root.attribute("width", "640").toInt();
    m_projectHeight = root.attribute("height", "480").toInt();
    
    QString bgColorStr = root.attribute("bgcolor", "#0");
    if (bgColorStr.startsWith("#")) {
        bool ok;
        int colorVal = bgColorStr.mid(1).toInt(&ok, 16);
        if (ok) {
            m_bgColor = QColor(colorVal & 0xFF, (colorVal >> 8) & 0xFF, (colorVal >> 16) & 0xFF);
        }
    }
    
    log(QString("Проект: %1x%2").arg(m_projectWidth).arg(m_projectHeight));
    
    // Обновляем заголовок окна
    setWindowTitle(QString("XML Editor - %1").arg(root.attribute("name", "Untitled")));
    
    // 1. Читаем схемы параметров
    parseParameters(root.firstChildElement("parameters"));
    
    // 2. Читаем объекты
    parseObjects(root);
    
    // 3. Обновляем все панели
    m_canvas->setCanvasSize(m_projectWidth, m_projectHeight);
    m_canvas->setBackgroundColor(m_bgColor);
    m_canvas->setShapes(m_shapes);
    
    m_objectList->setShapes(m_shapes);
    m_properties->clearProperties();
    
    log(QString("Загружено объектов: %1").arg(m_shapes.size()));

    return true;
}

void MainWindow::parseParameters(const QDomElement &paramsNode)
{
    if (paramsNode.isNull()) return;

    m_schemas.clear();
    
    QDomNode typeNode = paramsNode.firstChild();
    while (!typeNode.isNull()) {
        QDomElement typeEl = typeNode.toElement();
        if (!typeEl.isNull()) {
            QString typeName = typeEl.tagName();
            
            QMap<QString, ParamInfo> schema;
            
            QDomNode paramNode = typeEl.firstChild();
            while (!paramNode.isNull()) {
                QDomElement pEl = paramNode.toElement();
                if (!pEl.isNull()) {
                    ParamInfo info;
                    info.offset = pEl.attribute("offset").toInt();
                    info.size = pEl.attribute("size").toInt();
                    schema.insert(pEl.tagName(), info);
                }
                paramNode = paramNode.nextSibling();
            }
            m_schemas.insert(typeName, schema);
            log("Схема: " + typeName + " (" + QString::number(schema.size()) + " полей)");
        }
        typeNode = typeNode.nextSibling();
    }
}

void MainWindow::parseObjects(const QDomElement &root)
{
    m_shapes.clear();
    
    QDomElement objectsContainer = root.firstChildElement("objects");
    QDomNode searchNode = !objectsContainer.isNull() ? objectsContainer : root;
    QDomNode child = searchNode.firstChild();

    while (!child.isNull()) {
        QDomElement el = child.toElement();
        QString tagName = el.tagName();
        
        if (m_schemas.contains(tagName)) {
            QString hexInit = el.firstChildElement("init").text().trimmed();
            
            QSharedPointer<CorelShape> shape;

            if (tagName == "rectangle" || tagName == "rectanglea" || tagName == "rectanglee") {
                shape = QSharedPointer<CorelRect>::create();
            } 
            else if (tagName == "rotationobject") {
                shape = QSharedPointer<CorelRotationObject>::create();
            }

            if (shape && !hexInit.isEmpty()) {
                shape->parse(hexInit, m_schemas[tagName]);
                shape->parseExtraData(el);
                m_shapes.append(shape);
            }
        }
        child = child.nextSibling();
    }
}