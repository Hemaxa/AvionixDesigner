// OptimizedStaticImageObject — один объект сцены, экспортируемый в несколько обычных STATICGROUP.
#pragma once

#include "BaseObject.h"
#include "StaticGroupObject.h"
#include "editor/StaticImageDocument.h"

// OptimizedStaticImageObject — фиксированные пиксельные маски с общим положением.
class OptimizedStaticImageObject : public BaseObject
{
    Q_OBJECT
  public:
    double x = 0, y = 0;
    StaticImageDocument document;
    explicit OptimizedStaticImageObject(QObject *parent = nullptr) : BaseObject(parent) {}
    QSharedPointer<BaseObject> clone() const override;
    void parse(const QString &, const ParamSchema &) override {}
    void draw(QPainter &painter) override;
    QString getTypeName() const override
    {
        return QStringLiteral("OptimizedStaticImage");
    }
    QString getDisplayName() const override
    {
        return QStringLiteral("Статическое изображение");
    }
    bool isStaticImageOptimized() const override
    {
        return true;
    }
    bool canResize() const override
    {
        return false;
    }
    QRectF getBoundingRect() const override
    {
        return QRectF(QPointF(x, y), QSizeF(document.size));
    }
    void moveBy(double dx, double dy) override;
    QList<QPair<QString, QString>> getProperties() const override;
    bool setObjectProperty(const QString &name, const QString &value) override;
    QList<QSharedPointer<StaticGroupObject>> hardwareGroups() const;
    // Проверка байтов масок переиспользуется при перемещении; координаты проверяются отдельно.
    QString documentValidationError() const;
    void invalidatePreview()
    {
        m_preview = {};
        m_previewRegions = {};
        m_validationDocument = {};
        m_validationReady = false;
    }

  private:
    QImage m_preview;
    QList<StaticImageRegion> m_previewRegions;
    mutable StaticImageDocument m_validationDocument;
    mutable QString m_validationError;
    mutable bool m_validationReady = false;
};
