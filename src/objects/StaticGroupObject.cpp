/**
 * @file StaticGroupObject.cpp
 * @brief Реализация статической группы с несколькими состояниями
 */

#include "StaticGroupObject.h"
#include "BitParser.h"
#include "XmlReader.h"

StaticGroupObject::StaticGroupObject(QObject *parent)
    : BaseObject(parent)
{
}

GroupState StaticGroupObject::parseState(const QString &hexInit, const ParamSchema &schema)
{
    GroupState state;
    
    // Пиксельные координаты левого верхнего угла
    if (schema.contains("x"))
        state.x = BitParser::extract(hexInit, schema["x"].offset, schema["x"].size);
    if (schema.contains("y"))
        state.y = BitParser::extract(hexInit, schema["y"].offset, schema["y"].size);
    
    // Ширина и высота габаритного прямоугольника
    if (schema.contains("w"))
        state.w = BitParser::extract(hexInit, schema["w"].offset, schema["w"].size);
    if (schema.contains("h"))
        state.h = BitParser::extract(hexInit, schema["h"].offset, schema["h"].size);
    
    // Адрес смещения в блочной памяти
    if (schema.contains("addr"))
        state.addr = BitParser::extract(hexInit, schema["addr"].offset, schema["addr"].size);
    
    // Цвет
    if (schema.contains("color")) {
        quint32 colorVal = BitParser::extract(hexInit, schema["color"].offset, schema["color"].size);
        state.color = BitParser::parseColor(colorVal);
    }
    
    // Разрешение видимости
    if (schema.contains("enb"))
        state.enabled = BitParser::extract(hexInit, schema["enb"].offset, schema["enb"].size) != 0;
    
    return state;
}

void StaticGroupObject::parse(const QString &hexInit, const ParamSchema &schema)
{
    // Сохраняем схему для парсинга дополнительных init-строк в parseExtraData
    m_schema = schema;
    
    // Парсим первое состояние
    GroupState state = parseState(hexInit, schema);
    states.append(state);
}

void StaticGroupObject::parseExtraData(const QDomElement &element)
{
    // Читаем атрибут nomber (количество состояний)
    groupNumber = element.attribute("nomber", "0").toInt();
    
    // Парсим дополнительные init-строки (со 2-й и далее)
    QDomElement initEl = element.firstChildElement("init");
    bool firstSkipped = false;
    
    while (!initEl.isNull()) {
        if (!firstSkipped) {
            // Первый <init> уже распарсен в parse()
            firstSkipped = true;
        } else {
            QString hexInit = initEl.text().trimmed();
            if (!hexInit.isEmpty()) {
                GroupState state = parseState(hexInit, m_schema);
                states.append(state);
            }
        }
        initEl = initEl.nextSiblingElement("init");
    }
    
    // Парсим растровую маску из <data>
    QDomElement dataEl = element.firstChildElement("data");
    if (dataEl.isNull()) return;
    
    // Определяем размер маски из первого состояния
    if (states.isEmpty()) return;
    const GroupState &firstState = states[0];
    int w = firstState.w;
    int h = firstState.h;
    
    // Если в <data> есть атрибуты width/height, используем их
    if (dataEl.hasAttribute("width"))
        w = XmlReader::readInt(dataEl, "width", w);
    if (dataEl.hasAttribute("height"))
        h = XmlReader::readInt(dataEl, "height", h);
    
    QString text = dataEl.text().trimmed();
    if (w <= 0 || h <= 0 || text.isEmpty()) return;
    
    // Создаём изображение маски с цветом первого состояния
    maskImage = QImage(w, h, QImage::Format_ARGB32);
    maskImage.fill(Qt::transparent);
    
    QColor maskColor = firstState.color.isValid() ? firstState.color : Qt::white;
    
    QStringList parts = text.split(',');
    int idx = 0;
    
    for (int py = 0; py < h; ++py) {
        for (int px = 0; px < w; ++px) {
            if (idx >= parts.size()) break;
            
            int val = parts[idx].trimmed().toInt();
            idx++;
            
            if (val > 0) {
                int alpha8bit = (val * 255) / 7;
                QColor pixelColor = maskColor;
                pixelColor.setAlpha(alpha8bit);
                maskImage.setPixelColor(px, py, pixelColor);
            }
        }
    }
}

void StaticGroupObject::draw(QPainter &painter)
{
    if (states.isEmpty()) return;
    
    // Берём активное состояние (по умолчанию первое)
    int idx = qBound(0, activeState, states.size() - 1);
    const GroupState &state = states[idx];
    
    painter.save();
    
    if (!maskImage.isNull()) {
        // Рисуем маску в позиции состояния
        painter.drawImage(QPointF(state.x, state.y), maskImage);
    } else {
        // Если маски нет, рисуем рамку-заглушку
        QPen pen(Qt::gray, 1, Qt::DashLine);
        painter.setPen(pen);
        QColor fillColor = state.color.isValid() ? state.color.lighter(150) : Qt::lightGray;
        painter.setBrush(fillColor);
        painter.drawRect(QRectF(state.x, state.y, state.w, state.h));
        
        painter.setPen(Qt::darkGray);
        painter.drawText(QRectF(state.x, state.y, state.w, state.h), Qt::AlignCenter, 
                         QString("SG#%1").arg(groupNumber));
    }
    
    painter.restore();
}

QString StaticGroupObject::getTypeName() const
{
    return "StaticGroup";
}

QList<QPair<QString, QString>> StaticGroupObject::getProperties() const
{
    QList<QPair<QString, QString>> props;
    
    props.append({"Номер группы", QString::number(groupNumber)});
    props.append({"Кол-во состояний", QString::number(states.size())});
    props.append({"Активное", QString::number(activeState)});
    
    // Показываем свойства каждого состояния
    for (int i = 0; i < states.size(); ++i) {
        const GroupState &s = states[i];
        QString prefix = QString("Состояние %1: ").arg(i);
        props.append({prefix + "X", QString::number(s.x)});
        props.append({prefix + "Y", QString::number(s.y)});
        props.append({prefix + "Ширина", QString::number(s.w)});
        props.append({prefix + "Высота", QString::number(s.h)});
        props.append({prefix + "Цвет", s.color.name()});
        props.append({prefix + "Адрес", QString::number(s.addr)});
        props.append({prefix + "Видимость", s.enabled ? "да" : "нет"});
    }
    
    if (!maskImage.isNull()) {
        props.append({"Маска", QString("%1x%2").arg(maskImage.width()).arg(maskImage.height())});
    }
    
    return props;
}

QRectF StaticGroupObject::getBoundingRect() const
{
    if (states.isEmpty()) return QRectF();
    
    int idx = qBound(0, activeState, states.size() - 1);
    const GroupState &s = states[idx];
    return QRectF(s.x, s.y, s.w, s.h);
}

bool StaticGroupObject::setObjectProperty(const QString &name, const QString &value)
{
    bool ok = false;
    
    if (name == "Номер группы") {
        groupNumber = value.toInt(&ok);
    } else if (name == "Активное") {
        int val = value.toInt(&ok);
        if (ok && val >= 0 && val < states.size()) {
            activeState = val;
        }
    }
    
    if (ok) emit changed();
    return ok;
}
