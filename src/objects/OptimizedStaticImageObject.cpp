// OptimizedStaticImageObject — общая позиция, фиксированный размер и аппаратные группы частей.
#include "OptimizedStaticImageObject.h"
#include <cmath>

QSharedPointer<BaseObject> OptimizedStaticImageObject::clone() const
{
    auto copy = QSharedPointer<OptimizedStaticImageObject>::create();
    copy->x = x;
    copy->y = y;
    copy->document = document;
    copyEditorStateTo(*copy);
    return copy;
}

void OptimizedStaticImageObject::draw(QPainter &painter)
{
    if (m_preview.isNull())
        m_preview = document.render();
    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(QPoint(qRound(x), qRound(y)), m_preview);
    painter.restore();
}

void OptimizedStaticImageObject::moveBy(double dx, double dy)
{
    if (!std::isfinite(dx) || !std::isfinite(dy))
        return;
    x += dx;
    y += dy;
    emit changed();
}

QList<QPair<QString, QString>> OptimizedStaticImageObject::getProperties() const
{
    return {{"X", QString::number(x)},
            {"Y", QString::number(y)},
            {"Ширина", QString::number(document.size.width())},
            {"Высота", QString::number(document.size.height())},
            {"Частей", QString::number(document.exportedRegions().size())},
            {"Память", QStringLiteral("%1 ячеек, %2 init").arg(document.cells()).arg(document.initCount())}};
}

bool OptimizedStaticImageObject::setObjectProperty(const QString &name, const QString &value)
{
    bool ok = false;
    const double number = value.toDouble(&ok);
    if (!ok || !std::isfinite(number))
        return false;
    if (name == "X")
        x = number;
    else if (name == "Y")
        y = number;
    else
        return false;
    emit changed();
    return true;
}

QList<QSharedPointer<StaticGroupObject>> OptimizedStaticImageObject::hardwareGroups() const
{
    QList<QSharedPointer<StaticGroupObject>> groups;
    for (int index : document.exportedRegions())
    {
        auto group = QSharedPointer<StaticGroupObject>::create();
        int address = 0;
        for (const auto &mask : document.regions[index].masks)
        {
            group->states.append({qRound(x) + mask.rect.x(), qRound(y) + mask.rect.y(), mask.rect.width(),
                                  mask.rect.height(), address, mask.color, mask.enabled});
            group->maskImages.append(mask.image());
            address += mask.data.size();
        }
        group->groupNumber = group->states.size();
        groups.append(group);
    }
    return groups;
}
