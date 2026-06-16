#include "TextObject.h"

#include <QFont>
#include <QFontMetrics>
#include <QPainter>

TextObject::TextObject(QObject *parent) : StaticGroupObject(parent)
{
    GroupState state;
    state.x = 60;
    state.y = 60;
    state.color = QColor("#F8FAFC");
    state.enabled = true;
    states = {state};
    groupNumber = 1;
    rebuildMask();
}

QString TextObject::getTypeName() const
{
    return "Text";
}

QString TextObject::getDisplayName() const
{
    return "Текст";
}

QList<QPair<QString, QString>> TextObject::getProperties() const
{
    const GroupState state = states.isEmpty() ? GroupState{} : states.first();
    return {
        {"Текст", text},
        {"X", QString::number(state.x)},
        {"Y", QString::number(state.y)},
        {"Шрифт", fontFamily},
        {"Размер", QString::number(pixelSize)},
        {"Жирный", bold ? "да" : "нет"},
        {"Курсив", italic ? "да" : "нет"},
        {"Подчеркнутый", underline ? "да" : "нет"},
        {"Цвет", state.color.name()},
        {"Ширина", QString::number(state.w)},
        {"Высота", QString::number(state.h)}
    };
}

bool TextObject::setObjectProperty(const QString &name, const QString &value)
{
    if (states.isEmpty())
        return false;

    auto parseBool = [](const QString &raw, bool &target) {
        const QString value = raw.trimmed().toLower();
        if (value == "да" || value == "yes" || value == "1" || value == "true") {
            target = true;
            return true;
        }
        if (value == "нет" || value == "no" || value == "0" || value == "false") {
            target = false;
            return true;
        }
        return false;
    };

    bool ok = false;
    bool needsRebuild = false;
    GroupState &state = states[0];

    if (name == "Текст") {
        text = value;
        ok = true;
        needsRebuild = true;
    }
    else if (name == "X") {
        state.x = value.toInt(&ok);
    }
    else if (name == "Y") {
        state.y = value.toInt(&ok);
    }
    else if (name == "Шрифт") {
        fontFamily = value.trimmed();
        ok = !fontFamily.isEmpty();
        needsRebuild = ok;
    }
    else if (name == "Размер") {
        pixelSize = value.toInt(&ok);
        if (ok) {
            pixelSize = qMax(6, pixelSize);
            needsRebuild = true;
        }
    }
    else if (name == "Жирный") {
        ok = parseBool(value, bold);
        needsRebuild = ok;
    }
    else if (name == "Курсив") {
        ok = parseBool(value, italic);
        needsRebuild = ok;
    }
    else if (name == "Подчеркнутый") {
        ok = parseBool(value, underline);
        needsRebuild = ok;
    }
    else if (name == "Цвет") {
        const QColor color(value);
        ok = color.isValid();
        if (ok)
            state.color = color;
    }
    else if (name == "Ширина") {
        const int newWidth = qMax(1, value.toInt(&ok));
        if (ok) {
            state.w = newWidth;
            if (!maskImages.isEmpty() && !maskImages[0].isNull()) {
                maskImages[0] = maskImages[0].scaled(state.w, qMax(1, state.h), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            }
            rebuildStateAddresses();
        }
    }
    else if (name == "Высота") {
        const int newHeight = qMax(1, value.toInt(&ok));
        if (ok) {
            state.h = newHeight;
            if (!maskImages.isEmpty() && !maskImages[0].isNull()) {
                maskImages[0] = maskImages[0].scaled(qMax(1, state.w), state.h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            }
            rebuildStateAddresses();
        }
    }

    if (!ok)
        return false;

    if (needsRebuild) {
        rebuildMask();
    }

    emit changed();
    return true;
}

void TextObject::rebuildMask()
{
    if (states.isEmpty())
        return;

    GroupState &state = states[0];
    const QString drawText = text.isEmpty() ? QStringLiteral(" ") : text;

    QFont font(fontFamily, pixelSize);
    font.setPixelSize(pixelSize);
    font.setBold(bold);
    font.setItalic(italic);
    font.setUnderline(underline);

    QFontMetrics metrics(font);
    const QRect textRect = metrics.boundingRect(drawText);
    const int paddingX = qMax(4, pixelSize / 5);
    const int paddingY = qMax(4, pixelSize / 6);
    const int imageWidth = qMax(1, textRect.width() + paddingX * 2);
    const int imageHeight = qMax(1, metrics.height() + paddingY * 2);

    QImage image(imageWidth, imageHeight, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setFont(font);
    painter.setPen(QColor(255, 255, 255, 255));
    painter.drawText(QPoint(paddingX - textRect.left(), paddingY + metrics.ascent()), drawText);
    painter.end();

    state.w = image.width();
    state.h = image.height();
    state.enabled = true;
    groupNumber = 1;

    if (maskImages.isEmpty()) {
        maskImages.append(image);
    } else {
        maskImages[0] = image;
    }
    while (maskImages.size() > 1) {
        maskImages.removeLast();
    }

    rebuildStateAddresses();
}
