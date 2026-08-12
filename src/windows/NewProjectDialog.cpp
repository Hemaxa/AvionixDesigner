#include "NewProjectDialog.h"

#include <QColorDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
QLabel* createFormLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("DialogFieldLabel"));
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    label->setMinimumWidth(92);
    return label;
}

QIcon createColorSwatchIcon(const QColor &color)
{
    QPixmap pixmap(34, 22);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(color);
    painter.setPen(QPen(QColor(230, 245, 255, 170), 1));
    painter.drawRoundedRect(QRectF(1, 1, 32, 20), 5, 5);
    return QIcon(pixmap);
}
}

NewProjectDialog::NewProjectDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("NewProjectDialog"));
    setWindowTitle(QStringLiteral("Создать проект"));
    setModal(true);
    setMinimumWidth(430);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(22, 20, 22, 18);
    mainLayout->setSpacing(14);

    auto *titleLabel = new QLabel(QStringLiteral("Новый проект"), this);
    titleLabel->setObjectName(QStringLiteral("DialogTitleLabel"));
    mainLayout->addWidget(titleLabel);

    auto *description = new QLabel(
        QStringLiteral("Задайте рабочую область. XML-файл можно сохранить после создания проекта."),
        this
    );
    description->setObjectName(QStringLiteral("DialogDescriptionLabel"));
    description->setWordWrap(true);
    mainLayout->addWidget(description);

    auto *formLayout = new QGridLayout();
    formLayout->setContentsMargins(0, 2, 0, 0);
    formLayout->setHorizontalSpacing(14);
    formLayout->setVerticalSpacing(10);

    m_nameEdit = new QLineEdit(QStringLiteral("Untitled"), this);
    m_nameEdit->setFixedHeight(32);
    formLayout->addWidget(createFormLabel(QStringLiteral("Имя"), this), 0, 0);
    formLayout->addWidget(m_nameEdit, 0, 1);

    m_widthSpin = new QSpinBox(this);
    m_widthSpin->setRange(64, 4095);
    m_widthSpin->setValue(640);
    m_widthSpin->setFixedHeight(32);
    formLayout->addWidget(createFormLabel(QStringLiteral("Ширина"), this), 1, 0);
    formLayout->addWidget(m_widthSpin, 1, 1);

    m_heightSpin = new QSpinBox(this);
    m_heightSpin->setRange(64, 4095);
    m_heightSpin->setValue(480);
    m_heightSpin->setFixedHeight(32);
    formLayout->addWidget(createFormLabel(QStringLiteral("Высота"), this), 2, 0);
    formLayout->addWidget(m_heightSpin, 2, 1);

    m_colorButton = new QPushButton(QStringLiteral("Выбрать цвет"), this);
    m_colorButton->setObjectName(QStringLiteral("ProjectColorButton"));
    m_colorButton->setFixedHeight(32);
    formLayout->addWidget(createFormLabel(QStringLiteral("Фон"), this), 3, 0);
    formLayout->addWidget(m_colorButton, 3, 1);
    formLayout->setColumnStretch(1, 1);

    mainLayout->addLayout(formLayout);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Создать"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Отмена"));
    mainLayout->addWidget(buttons);

    refreshColorPreview();

    connect(m_colorButton, &QPushButton::clicked, this, &NewProjectDialog::chooseBackgroundColor);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString NewProjectDialog::projectName() const
{
    return m_nameEdit->text().trimmed();
}

int NewProjectDialog::canvasWidth() const
{
    return m_widthSpin->value();
}

int NewProjectDialog::canvasHeight() const
{
    return m_heightSpin->value();
}

QColor NewProjectDialog::backgroundColor() const
{
    return m_backgroundColor;
}

void NewProjectDialog::chooseBackgroundColor()
{
    const QColor selectedColor = QColorDialog::getColor(m_backgroundColor, this, QStringLiteral("Цвет фона"));
    if (!selectedColor.isValid())
        return;

    m_backgroundColor = selectedColor;
    refreshColorPreview();
}

void NewProjectDialog::refreshColorPreview()
{
    m_colorButton->setIcon(createColorSwatchIcon(m_backgroundColor));
    m_colorButton->setText(m_backgroundColor.name(QColor::HexRgb).toUpper());
}
