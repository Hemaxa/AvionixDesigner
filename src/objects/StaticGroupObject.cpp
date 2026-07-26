#include "StaticGroupObject.h"
#include "BitParser.h"
#include "ProportionalResize.h"
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

QString StaticGroupObject::getDisplayName() const
{
    return "static_group";
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
    bool first = true;
    for (const GroupState &s : states) {
        if (!s.enabled) continue;
        QRectF r(s.x, s.y, s.w, s.h);
        if (first) {
            result = r;
            first = false;
        } else {
            result = result.united(r);
        }
    }
    
    return result;
}

void StaticGroupObject::moveBy(double dx, double dy)
{
    // Двигаем все состояния вместе
    for (GroupState &s : states) {
        s.x += dx;
        s.y += dy;
    }
    emit changed();
}

void StaticGroupObject::resizeBy(int edgeFlags, double dx, double dy)
{
    if (!canResize())
        return;

    if (states.isEmpty())
        return;

    const QRectF oldBounds = getBoundingRect();
    if (oldBounds.isEmpty())
        return;

    const auto resized = proportionalResizeRect(oldBounds, edgeFlags, dx, dy);
    const QRectF newBounds = resized.rect;
    const double scaleX = resized.scale;
    const double scaleY = resized.scale;

    for (int i = 0; i < states.size(); ++i) {
        GroupState &s = states[i];
        const QRectF stateRect(s.x, s.y, s.w, s.h);

        const double relativeLeft = stateRect.left() - oldBounds.left();
        const double relativeTop = stateRect.top() - oldBounds.top();
        const double relativeRight = stateRect.right() - oldBounds.left();
        const double relativeBottom = stateRect.bottom() - oldBounds.top();

        const QRectF scaledRect(
            newBounds.left() + relativeLeft * scaleX,
            newBounds.top() + relativeTop * scaleY,
            qMax(1.0, stateRect.width() * scaleX),
            qMax(1.0, stateRect.height() * scaleY)
        );

        s.x = qRound(scaledRect.left());
        s.y = qRound(scaledRect.top());
        s.w = qMax(1, qRound(qMax(1.0, relativeRight * scaleX - relativeLeft * scaleX)));
        s.h = qMax(1, qRound(qMax(1.0, relativeBottom * scaleY - relativeTop * scaleY)));

        if (i < maskImages.size() && !maskImages[i].isNull()) {
            maskImages[i] = maskImages[i].scaled(s.w, s.h, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        }
    }

    rebuildStateAddresses();
    emit changed();
}

bool StaticGroupObject::setObjectProperty(const QString &name, const QString &value)
{
    bool ok = false;
    
    if (name == "Номер группы") {
        groupNumber = value.toInt(&ok);
    }
    else if (name.startsWith("Состояние ")) {
        // Парсинг "Состояние N: Свойство"
        int colonIdx = name.indexOf(':');
        if (colonIdx > 0) {
            QString stateStr = name.mid(10, colonIdx - 10);
            int stateIdx = stateStr.toInt(&ok);
            if (ok && stateIdx >= 0 && stateIdx < states.size()) {
                QString prop = name.mid(colonIdx + 2).trimmed();
                GroupState &s = states[stateIdx];
                
                if (prop == "X") s.x = value.toDouble(&ok);
                else if (prop == "Y") s.y = value.toDouble(&ok);
                else if (prop == "Ширина") {
                    if (!canResize()) {
                        setValidationMessage(QObject::tr("Размер растрового объекта заблокирован в ограниченном режиме."));
                        return false;
                    }
                    s.w = value.toDouble(&ok);
                }
                else if (prop == "Высота") {
                    if (!canResize()) {
                        setValidationMessage(QObject::tr("Размер растрового объекта заблокирован в ограниченном режиме."));
                        return false;
                    }
                    s.h = value.toDouble(&ok);
                }
                else if (prop == "Адрес") s.addr = value.toInt(&ok);
                else if (prop == "Цвет") {
                    s.color = QColor(value);
                    ok = s.color.isValid();
                }
                else if (prop == "Видимость") {
                    if (value.toLower() == "да" || value.toLower() == "yes" || value == "1") { s.enabled = true; ok = true; }
                    else if (value.toLower() == "нет" || value.toLower() == "no" || value == "0") { s.enabled = false; ok = true; }
                }
            }
        }
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

void StaticGroupObject::rebuildStateAddresses()
{
    int nextAddr = 0;
    for (GroupState &state : states) {
        state.addr = nextAddr;
        nextAddr += qMax(1, state.w) * qMax(1, state.h);
    }
}
