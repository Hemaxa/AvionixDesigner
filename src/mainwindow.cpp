#include "mainwindow.h"
#include <QFile>
#include <QDomDocument>
#include <QPainter>
#include <QFileDialog>
#include <QDebug>
#include <QSharedPointer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    resize(1024, 768);
    
    // Показываем диалог выбора файла при запуске
    QString fileName = QFileDialog::getOpenFileName(this, 
        "Выберите XML файл для отрисовки", 
        QString(), 
        "XML Files (*.xml)");
    
    if (!fileName.isEmpty()) {
        loadXml(fileName);
    } else {
        log("Файл не выбран.");
    }
}

MainWindow::~MainWindow() {}

void MainWindow::log(const QString &msg) {
    m_debugLog += msg + "\n";
    qDebug() << msg; 
    update();
}

bool MainWindow::loadXml(const QString &fileName)
{
    m_debugLog.clear();
    log("Загрузка: " + fileName);

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QDomDocument doc;
    if (!doc.setContent(&file)) {
        file.close();
        return false;
    }
    file.close();

    QDomElement root = doc.documentElement();
    
    // 1. Читаем все схемы
    parseParameters(root.firstChildElement("parameters"));
    
    // 2. Читаем объекты
    parseObjects(root);

    return true;
}

void MainWindow::parseParameters(const QDomElement &paramsNode)
{
    if (paramsNode.isNull()) return;

    // Перебираем типы фигур: rectangle, rotationobject и т.д.
    QDomNode typeNode = paramsNode.firstChild();
    while (!typeNode.isNull()) {
        QDomElement typeEl = typeNode.toElement();
        if (!typeEl.isNull()) {
            QString typeName = typeEl.tagName(); // "rectangle" или "rotationobject"
            
            QMap<QString, ParamInfo> schema;
            
            // Читаем поля внутри типа
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
            log("Загружена схема: " + typeName + " (" + QString::number(schema.size()) + " полей)");
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

    int count = 0;

    while (!child.isNull()) {
        QDomElement el = child.toElement();
        QString tagName = el.tagName();
        
        // Проверяем, есть ли у нас схема для этого типа
        if (m_schemas.contains(tagName)) {
            QString hexInit = el.firstChildElement("init").text().trimmed();
            
            QSharedPointer<CorelShape> shape;

            // ФАБРИКА ОБЪЕКТОВ
            if (tagName == "rectangle" || tagName == "rectanglea" || tagName == "rectanglee") {
                shape = QSharedPointer<CorelRect>::create();
            } 
            else if (tagName == "rotationobject") {
                shape = QSharedPointer<CorelRotationObject>::create();
            }

            if (shape && !hexInit.isEmpty()) {
                // 1. Парсим основные параметры из Hex
                shape->parse(hexInit, m_schemas[tagName]);
                
                // 2. Парсим дополнительные данные (например, маску для rotationobject)
                shape->parseExtraData(el);
                
                m_shapes.append(shape);
                count++;
                log("Добавлен объект: " + tagName);
            }
        }
        child = child.nextSibling();
    }
    
    if (count == 0) log("Фигуры не найдены.");
}

void MainWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), QColor(50, 50, 50)); 
    painter.setRenderHint(QPainter::Antialiasing);

    // Полиморфная отрисовка: просто вызываем draw для каждого объекта
    for (const auto &shape : m_shapes) {
        shape->draw(painter);
    }
    
    painter.setPen(Qt::white);
    painter.drawText(rect().adjusted(10,10,-10,-10), Qt::AlignLeft | Qt::AlignTop, m_debugLog);
}