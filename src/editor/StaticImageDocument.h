// StaticImageDocument — пиксельные маски и прямоугольное разбиение статического изображения.
#pragma once

#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QList>
#include <QRect>
#include <QString>

class BaseObject;

// StaticMaskAnalysis — содержимое маски и мелкие несвязанные фрагменты без изменения её данных.
struct StaticMaskAnalysis
{
    QRect bounds;
    QList<QRect> fragments;
    qint64 pixels = 0;
    int components = 0;
};

// StaticImageMask — один init; значения маски 0…7 хранятся без повторного сглаживания.
struct StaticImageMask
{
    QRect rect;
    QColor color;
    QByteArray data;
    bool enabled = true;
    StaticImageMask clipped(const QRect &bounds) const;
    QImage image() const;
    StaticMaskAnalysis analyze() const;
    static StaticImageMask fromImage(const QRect &rect, const QColor &color, const QImage &image,
                                     bool enabled = true);
};

// StaticImageRegion — прямоугольная часть с независимым порядком init и признаком исключения.
struct StaticImageRegion
{
    int id = 1;
    QString name;
    QRect rect;
    bool excluded = false;
    QList<StaticImageMask> masks;
    qint64 cells() const;
    bool empty() const;
};

// StaticImageDocument — черновик окна и сохраняемая модель оптимизированного объекта.
class StaticImageDocument
{
  public:
    QSize size;
    QList<StaticImageRegion> regions;
    qint64 originalCells = 0;
    int nextId = 2;

    static bool supports(const BaseObject *object);
    static QPointF objectOrigin(const BaseObject &object);
    static StaticImageDocument fromObject(const BaseObject &object, QString *error = nullptr);
    bool validate(QString *error = nullptr) const;
    QStringList hardwareErrors() const;
    int regionAt(const QPointF &point) const;
    QList<int> exportedRegions() const;
    qint64 cells() const;
    int initCount() const;
    bool split(int region, Qt::Orientation orientation, int coordinate);
    bool mergeRegions(QList<int> indexes);
    bool mergeMasks(int region, QList<int> indexes, const QColor &color);
    bool eraseMasks(int region, const QList<int> &indexes, QPointF from, QPointF to, int diameter);
    void excludeEmpty();
    QImage render(int region = -1, bool includeExcluded = false) const;
};
