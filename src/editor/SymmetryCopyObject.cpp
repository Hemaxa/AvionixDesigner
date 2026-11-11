// SymmetryCopyObject - отрисовка и сведения о зависимой копии без аппаратных параметров.

#include "SymmetryCopyObject.h"

SymmetryCopyObject::SymmetryCopyObject(QSharedPointer<BaseObject> source, QSharedPointer<SymmetryAxis> axis)
    : m_source(std::move(source)), m_axis(std::move(axis))
{
    setEditRestrictionHint(tr("Зависимая копия. Редактируйте исходник или нажмите значок копии в списке, чтобы сделать её независимой."));
    bindSource(m_source);
}

void SymmetryCopyObject::bindSource(const QSharedPointer<BaseObject> &source)
{
    if (m_source) disconnect(m_source.data(), nullptr, this, nullptr);
    m_source = source;
    if (m_source) connect(m_source.data(), &BaseObject::changed, this, &BaseObject::changed);
}

QTransform SymmetryCopyObject::reflection() const
{
    return m_axis->orientation == Qt::Vertical
        ? QTransform(-1, 0, 0, 1, 2 * m_axis->position, 0)
        : QTransform(1, 0, 0, -1, 0, 2 * m_axis->position);
}

void SymmetryCopyObject::draw(QPainter &painter)
{
    if (!m_source) return;
    painter.save();
    painter.setWorldTransform(reflection(), true);
    m_source->draw(painter);
    painter.restore();
}

QList<QPair<QString, QString>> SymmetryCopyObject::getProperties() const
{
    if (!m_source) return {};
    return {{tr("Исходный объект"), m_source->customName().isEmpty() ? m_source->getDisplayName() : m_source->customName()},
            {tr("Симметрия"), m_axis->orientation == Qt::Vertical ? tr("Вертикальная ось") : tr("Горизонтальная ось")},
            {tr("Координата оси"), QString::number(m_axis->position)},
            {tr("Состояние связи"), tr("Зависимая копия")}};
}
