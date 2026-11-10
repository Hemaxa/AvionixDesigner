// TextObject - редакторский объект текста, экспортируемый в FPGA XML через font/text ресурсы.

#pragma once

#include "StaticGroupObject.h"
#include "text/FontAtlas.h"

// TextObject - редакторский объект текста, экспортируемый в FPGA XML через font/text ресурсы.
class TextObject : public StaticGroupObject
{
    Q_OBJECT

public:
    QString text = QStringLiteral("TEXT");
    QString fontFamily = QStringLiteral("Arial");
    int pixelSize = 14;
    int fontIndex = 0;
    int symbolCapacity = 0; // Ноль выделяет по одной позиции на текущий код строки.
    QString extraCharacters = QStringLiteral("α°+-/");
    QStringList exportAlphabetGroups;
    bool restrictedAtlasEditing = false;

    explicit TextObject(QObject *parent = nullptr);

    QString getTypeName() const override;
    QString getDisplayName() const override;
    QList<QPair<QString, QString>> getProperties() const override;
    bool setObjectProperty(const QString &name, const QString &value) override;
    void resizeBy(int edgeFlags, double dx, double dy, bool proportional = false) override;
    QRectF getBoundingRect() const override;
    bool canUseText(const QString &candidate, QString *missingCharacters = nullptr) const;
    QString exportCharacters() const;
    void setFontAtlas(const FpgaFont &font, bool restricted);
    bool hasFontAtlas() const;
    const FpgaFont& fontAtlas() const;

private:
    void rebuildMask();

    FpgaFont m_fontAtlas;
    bool m_hasFontAtlas = false;
};
