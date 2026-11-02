//TextObject - редакторский объект текста, экспортируемый в FPGA XML через font/text ресурсы

#include "TextObject.h"

#include "ProportionalResize.h"
#include "text/FontRasterizer.h"
#include "text/TextLayout.h"

#include <QFont>
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

QString defaultSpecialCharacters()
{
    return QStringLiteral("α°+-/");
}

void appendUnique(QString *target, const QString &value)
{
    if (!target)
        return;

    for (const QChar ch : value) {
        if (!target->contains(ch))
            target->append(ch);
    }
}

QString normalizedSpecialCharacters(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("нет") || normalized == QStringLiteral("no")
        || normalized == QStringLiteral("0") || normalized == QStringLiteral("false")) {
        return {};
    }
    if (normalized == QStringLiteral("да") || normalized == QStringLiteral("yes")
        || normalized == QStringLiteral("1") || normalized == QStringLiteral("true")
        || normalized == QStringLiteral("all")) {
        return defaultSpecialCharacters();
    }

    QString result;
    for (const QChar ch : value) {
        if (defaultSpecialCharacters().contains(ch) && !result.contains(ch))
            result.append(ch);
    }

    if (normalized.contains(QStringLiteral("альфа")) || normalized.contains(QStringLiteral("alpha")))
        appendUnique(&result, QStringLiteral("α"));
    if (normalized.contains(QStringLiteral("град")) || normalized.contains(QStringLiteral("degree")))
        appendUnique(&result, QStringLiteral("°"));
    if (normalized.contains(QStringLiteral("плюс")) || normalized.contains(QStringLiteral("plus")))
        appendUnique(&result, QStringLiteral("+"));
    if (normalized.contains(QStringLiteral("минус")) || normalized.contains(QStringLiteral("minus")))
        appendUnique(&result, QStringLiteral("-"));
    if (normalized.contains(QStringLiteral("слеш")) || normalized.contains(QStringLiteral("slash")))
        appendUnique(&result, QStringLiteral("/"));

    return result;
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
        {"Символы", extraCharacters},
        {"X", QString::number(state.x)},
        {"Y", QString::number(state.y)},
        {"Шрифт", fontFamily},
        {"Размер", QString::number(pixelSize)},
        {"Индекс шрифта", QString::number(fontIndex)},
        {"Цвет", state.color.name()}
    };

    if (restrictedAtlasEditing) {
        props.append(qMakePair(QStringLiteral("Доступно символов"), QString::number(m_hasFontAtlas ? m_fontAtlas.glyphs.size() : 0)));
    }

    return props;
}

bool TextObject::setObjectProperty(const QString &name, const QString &value)
{
    if (states.isEmpty())
        return false;

    clearValidationMessage();

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
    else if (name == "Символы") {
        const QString characters = normalizedSpecialCharacters(value);
        if (restrictedAtlasEditing) {
            QString missing;
            if (!canUseText(characters, &missing)) {
                setValidationMessage(QStringLiteral("В импортированном XML нет символов: %1").arg(missing));
                return false;
            }
        }
        extraCharacters = characters;
        ok = true;
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
    if (states.isEmpty())
        return;

    QFont font(fontFamily);
    font.setPixelSize(pixelSize);
    const FpgaFont atlas = m_hasFontAtlas ? m_fontAtlas : rasterizeFont(font, text);
    TextLayout layout;
    QString error;
    if (!layoutText(text, atlas, &layout, &error)) {
        setValidationMessage(error);
        return;
    }

    GroupState &state = states[0];
    QImage image(layout.size, QImage::Format_ARGB32);
    if (image.isNull()) {
        setValidationMessage(QStringLiteral("Недостаточно памяти для предпросмотра текста."));
        return;
    }
    image.fill(Qt::transparent);
    QPainter painter(&image);
    for (const auto &positioned : layout.glyphs) {
        const auto glyph = atlas.glyphs.value(positioned.character);
        if (glyph.enabled)
            painter.drawImage(positioned.position, glyphMaskToImage(glyph, state.color));
    }
    painter.end();

    state.w = image.width();
    state.h = image.height();
    groupNumber = 1;
    maskImages = {image};
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
            pixel.setAlpha(qRound((level + 1) * 255.0 / 8.0));
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

QString TextObject::exportCharacters() const
{
    QString characters;
    appendUnique(&characters, text);
    appendUnique(&characters, extraCharacters);
    return characters;
}

void TextObject::setFontAtlas(const FpgaFont &font, bool restricted)
{
    m_fontAtlas = font;
    m_hasFontAtlas = true;
    restrictedAtlasEditing = restricted;
    fontIndex = font.index;
    fontFamily = font.name;
    pixelSize = font.size;
    exportAlphabetGroups.clear();
    extraCharacters.clear();
    for (const QChar ch : defaultSpecialCharacters()) {
        if (font.glyphs.contains(ch))
            extraCharacters.append(ch);
    }
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
    return QRect(state.x, state.y, state.w, state.h);
}

QRectF TextObject::getBoundingRect() const
{
    if (states.isEmpty())
        return QRectF();
    const GroupState state = states.first();
    return QRectF(state.x, state.y, state.w, state.h);
}
