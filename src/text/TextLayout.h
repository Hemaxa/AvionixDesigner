// TextLayout - общий расчёт расположения букв для редактора и аппаратного экспорта.

#pragma once

#include "text/FontAtlas.h"

#include <QPoint>
#include <QSize>

// PositionedGlyph - символ и рассчитанная позиция его маски внутри текстового блока.
struct PositionedGlyph
{
    QChar character;
    QPoint position;
};

// TextLayout - общий расчёт расположения букв для редактора и аппаратного экспорта.
struct TextLayout
{
    QList<PositionedGlyph> glyphs;
    QSize size = QSize(1, 1);
};

// Заполняет ненулевой layout координатами от верхнего левого угла текстового блока.
// Пробелы изменяют положение пера без создания маски; неизвестный глиф даёт ошибку.
bool layoutText(const QString &text, const FpgaFont &font, TextLayout *layout,
                QString *errorMessage = nullptr);

// TextMask - общая маска строки для предпросмотра и аппаратного экспорта.
struct TextMask
{
    QSize layoutSize;
    QPoint offset; // Смещение обрезанной маски относительно начала текстового блока.
    QSize size;
    QString data; // Построчные уровни ПЛИС: 0 прозрачен, m>0 имеет alpha=(m+1)/8.
};

// Смешивает глифы, обрезает внешние нулевые поля и проверяет площадь до выделения растра.
bool composeTextMask(const QString &text, const FpgaFont &font, TextMask *mask,
                     QString *errorMessage = nullptr, qint64 maxPixels = 64 * 1024 * 1024);
