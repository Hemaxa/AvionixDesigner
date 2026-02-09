/**
 * @file ObjectLibraryPanel.cpp
 * @brief Реализация панели библиотеки объектов
 */

#include "ObjectLibraryPanel.h"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QIcon>

ObjectLibraryPanel::ObjectLibraryPanel(QWidget *parent)
    : BasePanel(parent)
{
    // Устанавливаем имя для стилизации через QSS
    setPanelName("ObjectLibraryPanel");
    
    // Главный layout
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);
    
    // Создаём сетку кнопок
    createButtons();
    
    // Добавляем растяжку в конец
    mainLayout->addStretch();
}

void ObjectLibraryPanel::createButtons()
{
    // Создаём grid layout для кнопок
    m_gridLayout = new QGridLayout();
    m_gridLayout->setSpacing(6);
    
    // Пути к иконкам (будут добавлены пользователем в res/icons/library/)
    // Иконки загружаются из ресурсов Qt (:/ prefix)
    m_rectButton = createLibraryButton(":/icons/icons/library/rectangle.svg", "Прямоугольник");
    m_circleButton = createLibraryButton(":/icons/icons/library/circle.svg", "Круг");
    m_lineButton = createLibraryButton(":/icons/icons/library/line.svg", "Линия");
    m_polygonButton = createLibraryButton(":/icons/icons/library/polygon.svg", "Полигон");
    m_textButton = createLibraryButton(":/icons/icons/library/text.svg", "Текст");
    m_imageButton = createLibraryButton(":/icons/icons/library/image.svg", "Изображение");
    
    // Размещаем кнопки в сетке 2x3
    m_gridLayout->addWidget(m_rectButton, 0, 0);
    m_gridLayout->addWidget(m_circleButton, 0, 1);
    m_gridLayout->addWidget(m_lineButton, 0, 2);
    m_gridLayout->addWidget(m_polygonButton, 1, 0);
    m_gridLayout->addWidget(m_textButton, 1, 1);
    m_gridLayout->addWidget(m_imageButton, 1, 2);
    
    // Добавляем grid в главный layout
    static_cast<QVBoxLayout*>(layout())->insertLayout(0, m_gridLayout);
}

QPushButton* ObjectLibraryPanel::createLibraryButton(const QString &iconPath, const QString &tooltip)
{
    QPushButton *button = new QPushButton(this);
    button->setObjectName("LibraryButton");
    button->setToolTip(tooltip);
    
    // Квадратная кнопка 48x48
    button->setFixedSize(48, 48);
    
    // Пытаемся загрузить иконку
    QIcon icon(iconPath);
    if (!icon.isNull() && !icon.availableSizes().isEmpty()) {
        // Иконка найдена — устанавливаем её
        button->setIcon(icon);
        button->setIconSize(QSize(28, 28));
    } else {
        // Иконка не найдена — показываем первую букву tooltip как заглушку
        // (пользователь заменит на свои иконки)
        QString fallbackText = tooltip.isEmpty() ? "?" : tooltip.left(1).toUpper();
        button->setText(fallbackText);
        QFont font = button->font();
        font.setPointSize(16);
        font.setBold(true);
        button->setFont(font);
    }
    
    return button;
}

