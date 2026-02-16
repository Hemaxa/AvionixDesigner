#include "ViewportSettingsPanel.h"
#include "ProjectManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QColorDialog>

ViewportSettingsPanel::ViewportSettingsPanel(QWidget *parent) : BasePanel(parent)
{
    //устанавливаем имя для стилизации через QSS
    setPanelName("ViewportSettingsPanel");
    
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);
    
    //заголовок
    m_titleLabel = new QLabel("Настройки сцены", this);
    m_titleLabel->setObjectName("SettingsTitleLabel");
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    layout->addWidget(m_titleLabel);
    
    //размер рабочей области
    m_sizeLabel = new QLabel("Размер: —", this);
    m_sizeLabel->setObjectName("SettingsSizeLabel");
    layout->addWidget(m_sizeLabel);
    
    //цвет фона: строка с превью и кнопкой
    QHBoxLayout *bgRow = new QHBoxLayout();
    bgRow->setSpacing(6);
    
    QLabel *bgLabel = new QLabel("Фон:", this);
    bgRow->addWidget(bgLabel);
    
    m_bgColorPreview = new QLabel(this);
    m_bgColorPreview->setObjectName("BgColorPreview");
    m_bgColorPreview->setFixedSize(24, 24);
    m_bgColorPreview->setAutoFillBackground(true);
    bgRow->addWidget(m_bgColorPreview);
    
    m_bgColorButton = new QPushButton("Изменить", this);
    m_bgColorButton->setObjectName("BgColorButton");
    m_bgColorButton->setFixedHeight(24);
    bgRow->addWidget(m_bgColorButton);
    
    bgRow->addStretch();
    layout->addLayout(bgRow);
    
    layout->addStretch();
    
    //подключения
    connect(m_bgColorButton, &QPushButton::clicked, this, &ViewportSettingsPanel::onChangeBgColor);
    connect(ProjectManager::instance(), &ProjectManager::projectLoaded, this, &ViewportSettingsPanel::refreshInfo);
    
    //инициализация
    refreshInfo();
}

void ViewportSettingsPanel::refreshInfo()
{
    auto pm = ProjectManager::instance();
    int w = pm->getCanvasWidth();
    int h = pm->getCanvasHeight();
    
    if (w > 0 && h > 0) {
        m_sizeLabel->setText(QString("Размер: %1 × %2").arg(w).arg(h));
    } else {
        m_sizeLabel->setText("Размер: —");
    }
    
    QColor bg = pm->getBackgroundColor();
    QPalette pal = m_bgColorPreview->palette();
    pal.setColor(QPalette::Window, bg);
    m_bgColorPreview->setPalette(pal);
}

void ViewportSettingsPanel::onChangeBgColor()
{
    auto pm = ProjectManager::instance();
    QColor current = pm->getBackgroundColor();
    
    QColor newColor = QColorDialog::getColor(current, this, "Цвет фона");
    if (newColor.isValid() && newColor != current) {
        pm->setBackgroundColor(newColor);
        
        //обновляем превью
        QPalette pal = m_bgColorPreview->palette();
        pal.setColor(QPalette::Window, newColor);
        m_bgColorPreview->setPalette(pal);
        
        emit bgColorChanged(newColor);
    }
}
