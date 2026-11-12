// SymmetryCopyObject - зависимое отражение исходника, существующее только в редакторе.

#pragma once

#include "BaseObject.h"
#include <QSharedPointer>
#include <QTransform>

// SymmetryAxis - общая ось одного набора симметричных копий в координатах холста.
struct SymmetryAxis
{
    int id = 0;
    Qt::Orientation orientation = Qt::Vertical;
    double position = 0.0;
};

// SymmetryCopyObject - отображает исходник через отражение без дублирования его данных.
class SymmetryCopyObject : public BaseObject
{
    Q_OBJECT
public:
    SymmetryCopyObject(QSharedPointer<BaseObject> source, QSharedPointer<SymmetryAxis> axis);
    QSharedPointer<BaseObject> clone() const override;
    // При копировании всей сцены связи должны вести на копии исходника и оси.
    QSharedPointer<SymmetryCopyObject> cloneWithLinks(const QSharedPointer<BaseObject> &source,
                                                    const QSharedPointer<SymmetryAxis> &axis) const;
    void bindSource(const QSharedPointer<BaseObject> &source);
    const QSharedPointer<BaseObject> &source() const { return m_source; }
    const QSharedPointer<SymmetryAxis> &axis() const { return m_axis; }
    QTransform reflection() const;
    void parse(const QString &, const ParamSchema &) override {}
    void draw(QPainter &painter) override;
    QString getTypeName() const override { return m_source ? m_source->getTypeName() : QStringLiteral("symmetry-copy"); }
    QString getDisplayName() const override { return (m_source ? m_source->getDisplayName() : QStringLiteral("Объект")) + QStringLiteral(" — копия"); }
    QList<QPair<QString, QString>> getProperties() const override;
    QRectF getBoundingRect() const override { return m_source ? reflection().mapRect(m_source->getBoundingRect()) : QRectF(); }
    bool contains(const QPointF &point) const override { return m_source && m_source->contains(reflection().map(point)); }
    bool canMove() const override { return false; }
    bool canResize() const override { return false; }
    bool isExportEnabled() const override { return false; }
    void setExportEnabled(bool) override {}
private:
    QSharedPointer<BaseObject> m_source;
    QSharedPointer<SymmetryAxis> m_axis;
};
