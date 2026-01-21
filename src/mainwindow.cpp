#include "mainwindow.h"

#include <QFile>
#include <QDomDocument>
#include <QPainter>
#include <QFileDialog>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    resize(1000, 800);
    
    //пытаемся загрузить файл test.xml автоматически
    if (!loadFile("test.xml")) {
        //если не нашли, просим пользователя выбрать файл
        QString fileName = QFileDialog::getOpenFileName(this, "Выбрать XML", QString(), "XML (*.xml)");
        if (!fileName.isEmpty()) {
            loadFile(fileName);
        }
        else {
            log("Файл не выбран пользователем.");
        }
    }
}

MainWindow::~MainWindow() {}

//функция для добавления сообщений в лог на экране
void MainWindow::log(const QString &msg) {
    m_debugLog += msg + "\n";
    qDebug() << msg; 
    update();
}

//извлечение значения из hex-строки по битовой маске
quint32 MainWindow::extractValue(const QString &hexString, int offset, int size)
{
    quint32 result = 0;
    int len = hexString.length();
    
    for (int i = 0; i < size; ++i) {
        int targetBitIndex = offset + i; 
        
        //считаем индекс символа с конца строки
        int charIndex = len - 1 - (targetBitIndex / 4);
        
        if (charIndex < 0) break; 

        QChar ch = hexString[charIndex];
        int val = 0;
        
        //конвертируем символ hex в число 0-15
        if (ch >= '0' && ch <= '9') val = ch.toLatin1() - '0';
        else if (ch >= 'A' && ch <= 'F') val = ch.toLatin1() - 'A' + 10;
        else if (ch >= 'a' && ch <= 'f') val = ch.toLatin1() - 'a' + 10;

        //проверяем бит внутри символа
        if ((val >> (targetBitIndex % 4)) & 1) {
            result |= (1 << i);
        }
    }
    return result;
}

//метод загрузки файла
bool MainWindow::loadFile(const QString &fileName)
{
    m_debugLog.clear();
    log("Загрузка файла: " + fileName);

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        log("ОШИБКА: Не удалось открыть файл.");
        return false;
    }

    QDomDocument doc;
    QString errStr;
    int errLine;
    if (!doc.setContent(&file, false, &errStr, &errLine)) {
        log("ОШИБКА XML: " + errStr + " (строка " + QString::number(errLine) + ")");
        file.close();
        return false;
    }
    file.close();

    QDomElement root = doc.documentElement();
    log("Корневой элемент: <" + root.tagName() + ">");

    parseParameters(root.firstChildElement("parameters"));
    
    parseObjects(root);

    return true;
}

//метод загрузки параметров
void MainWindow::parseParameters(const QDomElement &paramsNode)
{
    if (paramsNode.isNull()) {
        log("ОШИБКА: Нет секции <parameters>!");
        return;
    }
    
    QDomElement rectSchema = paramsNode.firstChildElement("rectangle");
    if (rectSchema.isNull()) {
         log("ОШИБКА: В parameters нет описания для rectangle!");
         return;
    }

    QDomNode child = rectSchema.firstChild();
    while (!child.isNull()) {
        QDomElement el = child.toElement();
        if (!el.isNull()) {
            Parameter info;
            info.offset = el.attribute("offset").toInt();
            info.size = el.attribute("size").toInt();
            m_rectSchema.insert(el.tagName(), info);
        }
        child = child.nextSibling();
    }
    log("Схема загружена. Параметров: " + QString::number(m_rectSchema.size()));
}

void MainWindow::parseObjects(const QDomElement &root)
{
    m_rects.clear();
    
    //пытаемся найти контейнер <objects>, если его нет — ищем в корне
    QDomElement objectsContainer = root.firstChildElement("objects");
    QDomNode searchNode = !objectsContainer.isNull() ? objectsContainer : root;

    if (!objectsContainer.isNull()) log("Найден контейнер <objects>.");
    
    QDomNode child = searchNode.firstChild();
    int count = 0;

    while (!child.isNull()) {
        QDomElement el = child.toElement();

        //обработка прямоугольников
        if (el.tagName() == "rectangle") {
            QString hexInit = el.firstChildElement("init").text().trimmed();
            
            //если есть данные и схема загружена корректно
            if (!hexInit.isEmpty() && m_rectSchema.contains("x0")) {
                Rectangle r;
                
                //декодирование

                //1) координаты и размеры (делим на 10.0)
                int rawX = extractValue(hexInit, m_rectSchema["x0"].offset, m_rectSchema["x0"].size);
                int rawY = extractValue(hexInit, m_rectSchema["y0"].offset, m_rectSchema["y0"].size);
                int rawW = extractValue(hexInit, m_rectSchema["w"].offset, m_rectSchema["w"].size);
                int rawH = extractValue(hexInit, m_rectSchema["h"].offset, m_rectSchema["h"].size);
                
                r.x0 = rawX / 10.0;
                r.y0 = rawY / 10.0;
                r.w = rawW / 10.0;
                r.h = rawH / 10.0;

                //2) цвет заливки (color)
                quint32 cVal = extractValue(hexInit, m_rectSchema["color"].offset, m_rectSchema["color"].size);

                //BGR -> RGB
                r.color = QColor(cVal & 0xFF, (cVal >> 8) & 0xFF, (cVal >> 16) & 0xFF);

                //3) цвет обводки (colorb)
                if (m_rectSchema.contains("colorb")) {
                    quint32 cbVal = extractValue(hexInit, m_rectSchema["colorb"].offset, m_rectSchema["colorb"].size);
                    r.colorb = QColor(cbVal & 0xFF, (cbVal >> 8) & 0xFF, (cbVal >> 16) & 0xFF);
                }
                else {
                    r.colorb = Qt::black;
                }

                //4) толщина обводки (a)
                if (m_rectSchema.contains("a")) {
                    int rawA = extractValue(hexInit, m_rectSchema["a"].offset, m_rectSchema["a"].size);
                    r.a = rawA / 10.0;
                }
                else {
                    r.a = 1.0;
                }

                m_rects.append(r);
                count++;
                
                log("Rect #" + QString::number(count) + 
                    " X:" + QString::number(r.x0) + " Y:" + QString::number(r.y0) +
                    " W:" + QString::number(r.w) + " H:" + QString::number(r.h));
            }
        }
        child = child.nextSibling();
    }
    
    if (count == 0) log("ИТОГ: Фигуры не найдены.");
}

void MainWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    
    //фон
    painter.fillRect(rect(), QColor(50, 50, 50)); 

    painter.setRenderHint(QPainter::Antialiasing);

    for (const Rectangle &r : m_rects) {
        //подумать как лучше поступить с толщиной обводки
        QPen pen;
        if (r.a <= 0.05) {
            //если толщина почти 0 — считаем, что обводки нет
            pen.setStyle(Qt::NoPen);
        }
        else {
            pen.setColor(r.colorb);
            pen.setWidthF(r.a);
        }
        painter.setPen(pen);
        
        //заливка
        painter.setBrush(r.color);
        
        //рисование
        painter.drawRect(QRectF(r.x0, r.y0, r.w, r.h));
    }
    
    //вывод лога поверх всего
    painter.setPen(Qt::white);
    painter.drawText(rect().adjusted(10,10,-10,-10), Qt::AlignLeft | Qt::AlignTop, m_debugLog);
}