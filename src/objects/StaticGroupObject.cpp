// StaticGroupObject - класс группы статических растровых объектов с несколькими состояниями.

#include "StaticGroupObject.h"
#include "fpga/compiler/FpgaResourceLimits.h"
#include "BitParser.h"
#include "ProportionalResize.h"
#include <QtMath>

StaticGroupObject::StaticGroupObject(QObject *parent) : BaseObject(parent) {}

GroupState StaticGroupObject::parseState(const QString &hexInit, const ParamSchema &schema)
{
    GroupState state;

    // пиксельные координаты левого верхнего угла
    if (schema.contains("x"))
        state.x = BitParser::extract(hexInit, schema["x"].offset, schema["x"].size);
    if (schema.contains("y"))
        state.y = BitParser::extract(hexInit, schema["y"].offset, schema["y"].size);

    // ширина и высота габаритного прямоугольника
    if (schema.contains("w"))
        state.w = BitParser::extract(hexInit, schema["w"].offset, schema["w"].size);
    if (schema.contains("h"))
        state.h = BitParser::extract(hexInit, schema["h"].offset, schema["h"].size);

    // адрес смещения в блочной памяти
    if (schema.contains("addr"))
        state.addr = BitParser::extract(hexInit, schema["addr"].offset, schema["addr"].size);

    // цвет
    if (schema.contains("color")) {
        quint32 colorVal = BitParser::extract(hexInit, schema["color"].offset, schema["color"].size);
        state.color = BitParser::parseColor(colorVal);
    }

    // разрешение видимости
    if (schema.contains("enb"))
        state.enabled = BitParser::extract(hexInit, schema["enb"].offset, schema["enb"].size) != 0;

    return state;
}

void StaticGroupObject::parse(const QString &hexInit, const ParamSchema &schema)
{
    // сохраняем схему для парсинга дополнительных init-строк в parseExtraData
    m_schema = schema;

    // парсим первое состояние
    GroupState state = parseState(hexInit, schema);
    states.append(state);
}

void StaticGroupObject::parseExtraData(const QDomElement &element)
{
    // читаем атрибут nomber (количество состояний)
    groupNumber = element.attribute("nomber", "0").toInt();

    // парсим дополнительные init-строки (со 2-й и далее)
    QDomElement initEl = element.firstChildElement("init");
    bool firstSkipped = false;

    while (!initEl.isNull()) {
        if (!firstSkipped) {
            // первый <init> уже распарсен в parse()
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

    // парсим общую блочную память из <data>
    QDomElement dataEl = element.firstChildElement("data");
    if (dataEl.isNull() || states.isEmpty()) return;

    QString text = dataEl.text().trimmed();
    if (text.isEmpty()) return;

    // разбираем все пиксели в единый массив
    QStringList parts = text.split(',');
    QVector<int> sharedData;
    sharedData.reserve(parts.size());
    for (const QString &part : parts) {
        sharedData.append(part.trimmed().toInt());
    }

    // для каждого состояния создаём маску из общей памяти
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

    // рисуем все включённые состояния
    for (int i = 0; i < states.size(); ++i) {
        const GroupState &state = states[i];

        if (!state.enabled) continue;

        if (i < maskImages.size() && !maskImages[i].isNull()) {
            // рисуем растровую маску в позиции данного состояния
            painter.drawImage(QPointF(state.x, state.y), maskImages[i]);
        }
        else {
            // если маски нет, рисуем рамку-заглушку
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
    return QStringLiteral("Статическое изображение");
}

QList<QPair<QString, QString>> StaticGroupObject::getProperties() const
{
    QList<QPair<QString, QString>> props;

    props.append({"Номер группы", QString::number(groupNumber)});
    props.append({"Кол-во состояний", QString::number(states.size())});
    qint64 memoryEnd = 0;
    for (const auto &state : states)
        memoryEnd = qMax(memoryEnd, qint64(state.addr) + qint64(state.w) * state.h);
    props.append({QStringLiteral("Память"), QStringLiteral("%1/%3 ячеек, %2/%4 init").arg(memoryEnd).arg(states.size()).arg(FpgaResourceLimits::staticPixels()).arg(FpgaResourceLimits::staticParameters())});

    // показываем свойства каждого состояния
    for (int i = 0; i < states.size(); ++i) {
        const GroupState &s = states[i];
        QString prefix = QString("Состояние %1: ").arg(i);
        props.append({prefix + "X", QString::number(s.x)});
        props.append({prefix + "Y", QString::number(s.y)});
        props.append({prefix + "Ширина", QString::number(s.w)});
        props.append({prefix + "Высота", QString::number(s.h)});
        props.append({prefix + "Порядок", QString::number(i + 1)});
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

    // объединяем прямоугольники всех включённых состояний
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

void StaticGroupObject::resizeBy(int edgeFlags, double dx, double dy, bool proportional)
{
    Q_UNUSED(proportional);
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
    clearValidationMessage();
    bool ok = false;
    if (name == "Номер группы") {
        const int number = value.toInt(&ok);
        if (ok) groupNumber = number;
    } else if (name.startsWith("Состояние ")) {
        const int colon = name.indexOf(':');
        const int index = name.mid(10, colon - 10).toInt(&ok);
        if (!ok || colon < 10 || index < 0 || index >= states.size()) return false;
        const QString property = name.mid(colon + 1).trimmed();
        if (property == "Порядок") {
            const int destination = value.toInt(&ok) - 1;
            if (!ok || destination < 0 || destination >= states.size() || maskImages.size() != states.size()) return false;
            states.move(index, destination);
            maskImages.move(index, destination);
            rebuildStateAddresses();
            emit changed();
            return true;
        }
        GroupState state = states[index];
        const bool sizeChange = property == "Ширина" || property == "Высота";
        if (sizeChange && !canResize()) {
            setValidationMessage(tr("Размер растрового объекта заблокирован в ограниченном режиме."));
            return false;
        }
        if (property == "X" || property == "Y" || sizeChange) {
            const double number = value.toDouble(&ok);
            if (!ok || !qIsFinite(number) || number < -4095 || number > 4095 || (sizeChange && number < 1)) return false;
            if (property == "X") state.x = qRound(number);
            else if (property == "Y") state.y = qRound(number);
            else if (property == "Ширина") state.w = qRound(number);
            else state.h = qRound(number);
        } else if (property == "Адрес") {
            state.addr = value.toInt(&ok);
        } else if (property == "Цвет") {
            state.color = QColor(value); ok = state.color.isValid();
        } else if (property == "Видимость") {
            const auto normalized = value.trimmed().toLower();
            ok = normalized == "да" || normalized == "нет";
            state.enabled = normalized == "да";
        } else {
            return false;
        }
        if (!ok) return false;
        states[index] = state;
        if (index < maskImages.size() && !maskImages[index].isNull()) {
            if (sizeChange)
                maskImages[index] = maskImages[index].scaled(state.w,state.h,Qt::IgnoreAspectRatio,Qt::FastTransformation);
            if (property == "Цвет") {
                QImage image = maskImages[index].convertToFormat(QImage::Format_ARGB32);
                for (int y=0;y<image.height();++y) {
                    auto *row=reinterpret_cast<QRgb *>(image.scanLine(y));
                    for (int x=0;x<image.width();++x)
                        row[x]=qRgba(state.color.red(),state.color.green(),state.color.blue(),qAlpha(row[x]));
                }
                maskImages[index] = image;
            }
        }
        if (sizeChange) rebuildStateAddresses();
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

QSharedPointer<BaseObject> StaticGroupObject::clone() const
{
    auto *clone = new StaticGroupObject();
    clone->states = states;
    clone->groupNumber = groupNumber;
    clone->maskImages = maskImages;
    copyEditorStateTo(*clone);
    return QSharedPointer<BaseObject>(clone);
}
