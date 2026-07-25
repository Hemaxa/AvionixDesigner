//TextObject - редакторский объект текста, экспортируемый в FPGA XML через font/text ресурсы

#pragma once

#include "StaticGroupObject.h"

struct FpgaGlyph
{
    QChar literal;
    int code = 0;
    int width = 0;
    int height = 0;
    int advance = 0;
    int bearingX = 0;
    int bearingY = 0;
    int ascent = 0;
    int descent = 0;
    int floater = 0;
    int offset = 0;
    int size = 14;
    QString maskRows;
    QImage maskImage;
};

struct FpgaFont
{
    int index = 0;
    QString name = QStringLiteral("Arial");
    int size = 14;
    int volume = 0;
    QList<QChar> glyphOrder;
    QMap<QChar, FpgaGlyph> glyphs;
    QMap<QString, int> kerningPairs;
};

class TextObject : public StaticGroupObject
{
    Q_OBJECT

public:
    QString text = QStringLiteral("TEXT");
    QString fontFamily = QStringLiteral("Arial");
    int pixelSize = 14;
    int fontIndex = 0;
    QString extraCharacters = QStringLiteral("α°+-/");
    bool restrictedAtlasEditing = false;

    explicit TextObject(QObject *parent = nullptr);

    QString getTypeName() const override;
    QString getDisplayName() const override;
    QList<QPair<QString, QString>> getProperties() const override;
    bool setObjectProperty(const QString &name, const QString &value) override;
    void resizeBy(int edgeFlags, double dx, double dy) override;
    QRectF getBoundingRect() const override;
    bool canUseText(const QString &candidate, QString *missingCharacters = nullptr) const;
    QString exportCharacters() const;
    void setFontAtlas(const FpgaFont &font, bool restricted);
    bool hasFontAtlas() const;
    const FpgaFont& fontAtlas() const;
    QRect overallRect() const;

private:
    void rebuildMask();
    void rebuildMaskFromQtFont();
    void rebuildMaskFromAtlas();
    QImage glyphMaskToImage(const FpgaGlyph &glyph, const QColor &color) const;

    FpgaFont m_fontAtlas;
    bool m_hasFontAtlas = false;
};
