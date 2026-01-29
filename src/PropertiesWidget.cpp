#include "PropertiesWidget.h"
#include <QVBoxLayout>

PropertiesWidget::PropertiesWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumWidth(200);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    
    m_titleLabel = new QLabel("Свойства объекта", this);
    m_titleLabel->setStyleSheet("font-weight: bold; font-size: 12px;");
    mainLayout->addWidget(m_titleLabel);
    
    m_layout = new QFormLayout();
    m_layout->setSpacing(5);
    mainLayout->addLayout(m_layout);
    
    mainLayout->addStretch();
}

void PropertiesWidget::showProperties(QSharedPointer<CorelShape> shape)
{
    clearLayout();
    
    if (!shape) {
        m_titleLabel->setText("Объект не выбран");
        return;
    }
    
    // Проверяем тип объекта и показываем соответствующие свойства
    if (CorelRect *rect = dynamic_cast<CorelRect*>(shape.data())) {
        m_titleLabel->setText("Rectangle");
        addProperty("X", QString::number(rect->x, 'f', 1));
        addProperty("Y", QString::number(rect->y, 'f', 1));
        addProperty("Width", QString::number(rect->w, 'f', 1));
        addProperty("Height", QString::number(rect->h, 'f', 1));
        addProperty("Color", rect->color.name());
        addProperty("Stroke", rect->strokeColor.name());
        addProperty("Stroke Width", QString::number(rect->strokeWidth, 'f', 1));
        if (rect->alpha < 255) {
            addProperty("Alpha", QString::number(rect->alpha));
        }
    } 
    else if (CorelRotationObject *rot = dynamic_cast<CorelRotationObject*>(shape.data())) {
        m_titleLabel->setText("RotationObject");
        addProperty("Left", QString::number(rot->left, 'f', 1));
        addProperty("Top", QString::number(rot->top, 'f', 1));
        addProperty("Right", QString::number(rot->right, 'f', 1));
        addProperty("Bottom", QString::number(rot->bottom, 'f', 1));
        addProperty("X Rotation", QString::number(rot->xRot, 'f', 1));
        addProperty("Y Rotation", QString::number(rot->yRot, 'f', 1));
        addProperty("Color", rot->color.name());
        if (!rot->maskImage.isNull()) {
            addProperty("Mask Size", QString("%1x%2").arg(rot->maskImage.width()).arg(rot->maskImage.height()));
        }
    }
    else {
        m_titleLabel->setText("Unknown Object");
    }
}

void PropertiesWidget::clearProperties()
{
    clearLayout();
    m_titleLabel->setText("Свойства объекта");
}

void PropertiesWidget::addProperty(const QString &name, const QString &value)
{
    QLabel *valueLabel = new QLabel(value, this);
    valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_layout->addRow(name + ":", valueLabel);
}

void PropertiesWidget::clearLayout()
{
    while (m_layout->count() > 0) {
        QLayoutItem *item = m_layout->takeAt(0);
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
}
