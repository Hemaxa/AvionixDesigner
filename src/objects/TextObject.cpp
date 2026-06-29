#include "TextObject.h"

#include "ProportionalResize.h"

#include <QFont>
#include <QFontMetrics>
#include <QPainter>

namespace {
QStringList basicFontFamilies()
{
    return {
        QStringLiteral("Arial"),
        QStringLiteral("Times New Roman"),
        QStringLiteral("Helvetica"),
        QStringLiteral("Courier New"),
        QStringLiteral("Verdana"),
        QStringLiteral("Tahoma"),
        QStringLiteral("Georgia")
    };
}
}

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
    QList<QPair<QString, QString>> props = {
        {"Текст", text},
        {"X", QString::number(state.x)},
        {"Y", QString::number(state.y)},
        {"Шрифт", fontFamily},
        {"Размер", QString::number(pixelSize)},
        {"Индекс шрифта", QString::number(fontIndex)},
        {"Цвет", state.color.name()}
    };

    if (restrictedAtlasEditing) {
        props.append(qMakePair(QStringLiteral("Доступно символов"), QString::number(m_hasFontAtlas ? m_fontAtlas.glyphs.size() : 0)));
    } else {
        props.append(qMakePair(QStringLiteral("Жирный"), bold ? QStringLiteral("да") : QStringLiteral("нет")));
        props.append(qMakePair(QStringLiteral("Курсив"), italic ? QStringLiteral("да") : QStringLiteral("нет")));
        props.append(qMakePair(QStringLiteral("Подчеркнутый"), underline ? QStringLiteral("да") : QStringLiteral("нет")));
    }

    return props;
}

bool TextObject::setObjectProperty(const QString &name, const QString &value)
{
    if (states.isEmpty())
        return false;

    clearValidationMessage();

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
        if (restrictedAtlasEditing) {
            QString missing;
            if (!canUseText(value, &missing)) {
                setValidationMessage(QStringLiteral("В импортированном XML нет символов: %1").arg(missing));
                return false;
            }
        }
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
        if (restrictedAtlasEditing || isImportedHardwareObject()) {
            setValidationMessage(QStringLiteral("Шрифт заблокирован: в XML уже зашит фиксированный атлас."));
            return false;
        }
        if (!basicFontFamilies().contains(value.trimmed())) {
            setValidationMessage(QStringLiteral("Выберите один из базовых шрифтов."));
            return false;
        }
        fontFamily = value.trimmed();
        ok = !fontFamily.isEmpty();
        needsRebuild = ok;
    }
    else if (name == "Размер") {
        if (restrictedAtlasEditing || isImportedHardwareObject()) {
            setValidationMessage(QStringLiteral("Размер шрифта заблокирован: в XML уже зашит фиксированный атлас."));
            return false;
        }
        pixelSize = value.toInt(&ok);
        if (ok) {
            if (pixelSize < 6 || pixelSize > 128) {
                setValidationMessage(QStringLiteral("Размер шрифта должен быть от 6 до 128 пикселей."));
                return false;
            }
            needsRebuild = true;
        }
    }
    else if (name == "Индекс шрифта") {
        if (restrictedAtlasEditing || isImportedHardwareObject()) {
            setValidationMessage(QStringLiteral("Индекс шрифта заблокирован для импортированного XML."));
            return false;
        }
        fontIndex = value.toInt(&ok);
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
        if (ok) {
            state.color = color;
            needsRebuild = true;
        }
    }
    else if (name == "Доступно символов") {
        setValidationMessage(QStringLiteral("Это информационное поле."));
        return false;
    }

    if (!ok)
        return false;

    if (needsRebuild) {
        rebuildMask();
    }

    emit changed();
    return true;
}

void TextObject::resizeBy(int edgeFlags, double dx, double dy)
{
    if (restrictedAtlasEditing || !canResize())
        return;

    if (states.isEmpty())
        return;

    GroupState &state = states[0];
    const QRectF oldBounds(state.x, state.y, qMax(1, state.w), qMax(1, state.h));
    const int oldRight = state.x + state.w;
    const int oldBottom = state.y + state.h;
    const auto resized = proportionalResizeRect(oldBounds, edgeFlags, dx, dy);

    pixelSize = qBound(6, qRound(pixelSize * resized.scale), 128);
    rebuildMask();

    if (states.isEmpty())
        return;

    GroupState &updatedState = states[0];
    if ((edgeFlags & 1) && !(edgeFlags & 2)) {
        updatedState.x = oldRight - updatedState.w;
    } else if ((edgeFlags & 2) && !(edgeFlags & 1)) {
        updatedState.x = qRound(oldBounds.left());
    } else {
        updatedState.x = qRound(resized.rect.center().x() - updatedState.w / 2.0);
    }

    if ((edgeFlags & 4) && !(edgeFlags & 8)) {
        updatedState.y = oldBottom - updatedState.h;
    } else if ((edgeFlags & 8) && !(edgeFlags & 4)) {
        updatedState.y = qRound(oldBounds.top());
    } else {
        updatedState.y = qRound(resized.rect.center().y() - updatedState.h / 2.0);
    }

    rebuildStateAddresses();
    emit changed();
}

void TextObject::rebuildMask()
{
    // Всегда рендерим через системный QFont для гладкого отображения в viewport.
    // Атлас (rebuildMaskFromAtlas) используется исключительно для экспорта <data>.
    rebuildMaskFromQtFont();
}

void TextObject::rebuildMaskFromQtFont()
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
    const int paddingX = qMax(2, pixelSize / 6);
    const int paddingY = qMax(2, pixelSize / 7);
    const int imageWidth = qMax(1, textRect.width() + paddingX * 2);
    const int imageHeight = qMax(1, metrics.height() + paddingY * 2);

    QImage image(imageWidth, imageHeight, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setFont(font);
    painter.setPen(state.color);
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

void TextObject::rebuildMaskFromAtlas()
{
    if (states.isEmpty() || !m_hasFontAtlas)
        return;

    GroupState &state = states[0];
    const QColor color = state.color.isValid() ? state.color : QColor(Qt::white);
    const QString drawText = text.isEmpty() ? QStringLiteral(" ") : text;

    int totalWidth = 0;
    int top = 0;
    int bottom = 0;
    bool firstGlyph = true;
    QChar previous;

    for (const QChar ch : drawText) {
        if (ch.isSpace()) {
            totalWidth += qMax(4, pixelSize / 2);
            previous = ch;
            continue;
        }
        const FpgaGlyph glyph = m_fontAtlas.glyphs.value(ch);
        if (glyph.width <= 0 || glyph.height <= 0)
            continue;

        if (!previous.isNull())
            totalWidth += m_fontAtlas.kerningPairs.value(QString(previous) + QString(ch), 0);
        totalWidth += glyph.advance;
        const int glyphTop = glyph.floater;
        const int glyphBottom = glyph.floater + glyph.height;
        if (firstGlyph) {
            top = glyphTop;
            bottom = glyphBottom;
            firstGlyph = false;
        } else {
            top = qMin(top, glyphTop);
            bottom = qMax(bottom, glyphBottom);
        }
        previous = ch;
    }

    totalWidth = qMax(1, totalWidth);
    const int imageHeight = qMax(1, bottom - top);
    QImage image(totalWidth, imageHeight, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    int currentX = 0;
    previous = QChar();
    for (const QChar ch : drawText) {
        if (ch.isSpace()) {
            currentX += qMax(4, pixelSize / 2);
            previous = ch;
            continue;
        }

        const FpgaGlyph glyph = m_fontAtlas.glyphs.value(ch);
        if (glyph.width <= 0 || glyph.height <= 0)
            continue;

        if (!previous.isNull())
            currentX += m_fontAtlas.kerningPairs.value(QString(previous) + QString(ch), 0);
        painter.drawImage(QPoint(currentX, glyph.floater - top), glyphMaskToImage(glyph, color));
        currentX += glyph.advance;
        previous = ch;
    }
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

QImage TextObject::glyphMaskToImage(const FpgaGlyph &glyph, const QColor &color) const
{
    QImage image(qMax(1, glyph.width), qMax(1, glyph.height), QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    const QStringList rows = glyph.maskRows.split('\n', Qt::SkipEmptyParts);
    for (int y = 0; y < qMin(glyph.height, rows.size()); ++y) {
        const QString row = rows[y].trimmed();
        for (int x = 0; x < qMin(glyph.width, row.size()); ++x) {
            const int level = row[x].digitValue();
            if (level <= 0)
                continue;

            QColor pixel = color;
            pixel.setAlpha(qBound(0, level * 255 / 7, 255));
            image.setPixelColor(x, y, pixel);
        }
    }
    return image;
}

bool TextObject::canUseText(const QString &candidate, QString *missingCharacters) const
{
    if (!m_hasFontAtlas)
        return true;

    QString missing;
    for (const QChar ch : candidate) {
        if (ch.isSpace())
            continue;
        if (!m_fontAtlas.glyphs.contains(ch) && !missing.contains(ch))
            missing.append(ch);
    }

    if (missingCharacters)
        *missingCharacters = missing;
    return missing.isEmpty();
}

void TextObject::setFontAtlas(const FpgaFont &font, bool restricted)
{
    m_fontAtlas = font;
    m_hasFontAtlas = true;
    restrictedAtlasEditing = restricted;
    fontIndex = font.index;
    fontFamily = font.name;
    pixelSize = font.size;
    setImportedHardwareObject(restricted);
    setResizeLocked(restricted);
    if (restricted) {
        setEditRestrictionHint(QStringLiteral("Текст импортирован из XML: шрифт и размер зафиксированы, доступны только символы из атласа."));
    }
    rebuildMask();
}

bool TextObject::hasFontAtlas() const
{
    return m_hasFontAtlas;
}

const FpgaFont& TextObject::fontAtlas() const
{
    return m_fontAtlas;
}

QRect TextObject::overallRect() const
{
    const GroupState state = states.isEmpty() ? GroupState{} : states.first();
    if (!m_hasFontAtlas)
        return QRect(state.x, state.y, state.w, state.h);

    const QString drawText = text.isEmpty() ? QStringLiteral(" ") : text;
    int currentX = 0;
    int top = 0;
    int bottom = 0;
    int lastGlyphRight = 0;
    bool firstGlyph = true;
    QChar previous;

    for (const QChar ch : drawText) {
        if (ch.isSpace()) {
            currentX += qMax(4, pixelSize / 2);
            previous = ch;
            continue;
        }
        const FpgaGlyph glyph = m_fontAtlas.glyphs.value(ch);
        if (glyph.width <= 0 || glyph.height <= 0)
            continue;

        if (!previous.isNull())
            currentX += m_fontAtlas.kerningPairs.value(QString(previous) + QString(ch), 0);

        const int glyphTop = glyph.floater;
        const int glyphBottom = glyph.floater + glyph.height;
        lastGlyphRight = currentX + glyph.width;

        if (firstGlyph) {
            top = glyphTop;
            bottom = glyphBottom;
            firstGlyph = false;
        } else {
            top = qMin(top, glyphTop);
            bottom = qMax(bottom, glyphBottom);
        }

        currentX += glyph.advance;
        previous = ch;
    }

    return QRect(state.x, state.y + top, qMax(1, lastGlyphRight), qMax(1, bottom - top));
}

QRectF TextObject::getBoundingRect() const
{
    if (states.isEmpty())
        return QRectF();
    const GroupState state = states.first();
    return QRectF(state.x, state.y, state.w, state.h);
}
