#include "NewProjectDialog.h"

#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

NewProjectDialog::NewProjectDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Создать проект");
    setModal(true);
    resize(440, 0);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    auto *description = new QLabel(
        "Укажите параметры новой рабочей области. Проект будет сохранён как редактируемый .avd, а XML для ПЛИС можно экспортировать отдельно.",
        this
    );
    description->setWordWrap(true);
    mainLayout->addWidget(description);

    auto *formLayout = new QFormLayout();
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_nameEdit = new QLineEdit("Untitled", this);
    formLayout->addRow("Имя проекта", m_nameEdit);

    m_widthSpin = new QSpinBox(this);
    m_widthSpin->setRange(64, 4095);
    m_widthSpin->setValue(640);
    formLayout->addRow("Ширина", m_widthSpin);

    m_heightSpin = new QSpinBox(this);
    m_heightSpin->setRange(64, 4095);
    m_heightSpin->setValue(480);
    formLayout->addRow("Высота", m_heightSpin);

    auto *colorRow = new QHBoxLayout();
    colorRow->setSpacing(8);

    m_colorPreview = new QLabel(this);
    m_colorPreview->setFixedSize(28, 28);
    m_colorPreview->setAutoFillBackground(true);
    colorRow->addWidget(m_colorPreview);

    m_colorButton = new QPushButton("Выбрать цвет", this);
    colorRow->addWidget(m_colorButton);
    colorRow->addStretch();
    formLayout->addRow("Фон", colorRow);

    auto *pathRow = new QHBoxLayout();
    pathRow->setSpacing(8);
    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setPlaceholderText("Необязательно, можно сохранить позже");
    pathRow->addWidget(m_pathEdit);

    auto *browseButton = new QPushButton("Обзор...", this);
    pathRow->addWidget(browseButton);
    formLayout->addRow("Файл проекта", pathRow);

    mainLayout->addLayout(formLayout);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttons);

    refreshColorPreview();

    connect(m_colorButton, &QPushButton::clicked, this, &NewProjectDialog::chooseBackgroundColor);
    connect(browseButton, &QPushButton::clicked, this, &NewProjectDialog::chooseFilePath);
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

QString NewProjectDialog::filePath() const
{
    return m_pathEdit->text().trimmed();
}

void NewProjectDialog::chooseBackgroundColor()
{
    const QColor selectedColor = QColorDialog::getColor(m_backgroundColor, this, "Цвет фона");
    if (!selectedColor.isValid())
        return;

    m_backgroundColor = selectedColor;
    refreshColorPreview();
}

void NewProjectDialog::chooseFilePath()
{
    const QString fileName = QFileDialog::getSaveFileName(
        this,
        "Создать проект Avionix Designer",
        m_pathEdit->text().trimmed(),
        "Avionix Designer (*.avd)"
    );

    if (!fileName.isEmpty()) {
        m_pathEdit->setText(fileName);
    }
}

void NewProjectDialog::refreshColorPreview()
{
    QPalette palette = m_colorPreview->palette();
    palette.setColor(QPalette::Window, m_backgroundColor);
    m_colorPreview->setPalette(palette);
}
