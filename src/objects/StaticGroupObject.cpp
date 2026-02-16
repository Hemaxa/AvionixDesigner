#include "StaticGroupObject.h"
#include "BitParser.h"
#include "XmlReader.h"

StaticGroupObject::StaticGroupObject(QObject *parent) : BaseObject(parent) {}

GroupState StaticGroupObject::parseState(const QString &hexInit, const ParamSchema &schema)
{
    GroupState state;
    
    //пиксельные координаты левого верхнего угла
    if (schema.contains("x"))
        state.x = BitParser::extract(hexInit, schema["x"].offset, schema["x"].size);
    if (schema.contains("y"))
        state.y = BitParser::extract(hexInit, schema["y"].offset, schema["y"].size);
    
    //ширина и высота габаритного прямоугольника
    if (schema.contains("w"))
        state.w = BitParser::extract(hexInit, schema["w"].offset, schema["w"].size);
    if (schema.contains("h"))
        state.h = BitParser::extract(hexInit, schema["h"].offset, schema["h"].size);
    
    //адрес смещения в блочной памяти
    if (schema.contains("addr"))
        state.addr = BitParser::extract(hexInit, schema["addr"].offset, schema["addr"].size);
    
    //цвет
    if (schema.contains("color")) {
        quint32 colorVal = BitParser::extract(hexInit, schema["color"].offset, schema["color"].size);
        state.color = BitParser::parseColor(colorVal);
    }
    
    //разрешение видимости
    if (schema.contains("enb"))
        state.enabled = BitParser::extract(hexInit, schema["enb"].offset, schema["enb"].size) != 0;
    
    return state;
}

void StaticGroupObject::parse(const QString &hexInit, const ParamSchema &schema)
{
    //сохраняем схему для парсинга дополнительных init-строк в parseExtraData
    m_schema = schema;
    
    //парсим первое состояние
    GroupState state = parseState(hexInit, schema);
    states.append(state);
}

void StaticGroupObject::parseExtraData(const QDomElement &element)
{
    //читаем атрибут nomber (количество состояний)
    groupNumber = element.attribute("nomber", "0").toInt();
    
    //парсим дополнительные init-строки (со 2-й и далее)
    QDomElement initEl = element.firstChildElement("init");
    bool firstSkipped = false;
    
    while (!initEl.isNull()) {
        if (!firstSkipped) {
            //первый <init> уже распарсен в parse()
            firstSkipped = true;
        }
        else {
            QString hexInit = initEl.text().trimmed();
            if (!hexInit.isEmpty()) {
                GroupState state = parseState(hexInit, m_schema);
                states.append(state);
            }
        }
        initEl = initEl.nextSiblingElement("init");
    }
    
    //парсим общую блочную память из <data>
    QDomElement dataEl = element.firstChildElement("data");
    if (dataEl.isNull() || states.isEmpty()) return;
    
    QString text = dataEl.text().trimmed();
    if (text.isEmpty()) return;
    
    //разбираем все пиксели в единый массив
    QStringList parts = text.split(',');
    QVector<int> sharedData;
    sharedData.reserve(parts.size());
    for (const QString &part : parts) {
        sharedData.append(part.trimmed().toInt());
    }
    
    //для каждого состояния создаём маску из общей памяти
    for (int i = 0; i < states.size(); ++i) {
        GroupState &state = states[i];
        int w = state.w;
        int h = state.h;
        int startAddr = state.addr;
        
        if (w <= 0 || h <= 0) continue;
        
        QImage mask(w, h, QImage::Format_ARGB32);
        mask.fill(Qt::transparent);
        
        QColor maskColor = state.color.isValid() ? state.color : Qt::white;
        
        int idx = startAddr;
        for (int py = 0; py < h; ++py) {
            for (int px = 0; px < w; ++px) {
                if (idx >= sharedData.size()) break;
                
                int val = sharedData[idx];
                idx++;
                
                if (val > 0) {
                    int alpha8bit = (val * 255) / 7;
                    QColor pixelColor = maskColor;
                    pixelColor.setAlpha(alpha8bit);
                    mask.setPixelColor(px, py, pixelColor);
                }
            }
        }
        
        maskImages.append(mask);
    }
}

void StaticGroupObject::draw(QPainter &painter)
{
    if (states.isEmpty()) return;
    
    painter.save();
    
    //рисуем все включённые состояния
    for (int i = 0; i < states.size(); ++i) {
        const GroupState &state = states[i];
        
        if (!state.enabled) continue;
        
        if (i < maskImages.size() && !maskImages[i].isNull()) {
            //рисуем растровую маску в позиции данного состояния
            painter.drawImage(QPointF(state.x, state.y), maskImages[i]);
        }
        else {
            //если маски нет, рисуем рамку-заглушку
            QPen pen(Qt::gray, 1, Qt::DashLine);
            painter.setPen(pen);
            QColor fillColor = state.color.isValid() ? state.color.lighter(150) : Qt::lightGray;
            painter.setBrush(fillColor);
            painter.drawRect(QRectF(state.x, state.y, state.w, state.h));
        }
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
    
    //показываем свойства каждого состояния
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
        
        if (i < maskImages.size() && !maskImages[i].isNull()) {
            props.append({prefix + "Маска", QString("%1x%2").arg(maskImages[i].width()).arg(maskImages[i].height())});
        }
    }
    
    return props;
}

QRectF StaticGroupObject::getBoundingRect() const
{
    if (states.isEmpty()) return QRectF();
    
    //объединяем прямоугольники всех включённых состояний
    QRectF result;
    for (const GroupState &s : states) {
        if (!s.enabled) continue;
        QRectF r(s.x, s.y, s.w, s.h);
        if (result.isNull())
            result = r;
        else
            result = result.united(r);
    }
    
    return result;
}

bool StaticGroupObject::setObjectProperty(const QString &name, const QString &value)
{
    bool ok = false;
    
    if (name == "Номер группы") {
        groupNumber = value.toInt(&ok);
    }
    
    if (ok) emit changed();
    return ok;
}

QMap<QString, quint32> StaticGroupObject::serializeParams() const
{
    return serializeState(0);
}

QMap<QString, quint32> StaticGroupObject::serializeState(int stateIndex) const
{
    if (stateIndex < 0 || stateIndex >= states.size()) return {};
    
    const GroupState &s = states[stateIndex];
    return {
        {"x", static_cast<quint32>(s.x)},
        {"y", static_cast<quint32>(s.y)},
        {"w", static_cast<quint32>(s.w)},
        {"h", static_cast<quint32>(s.h)},
        {"addr", static_cast<quint32>(s.addr)},
        {"color", BitParser::colorToBgr(s.color)},
        {"enb", s.enabled ? 1u : 0u}
    };
}
