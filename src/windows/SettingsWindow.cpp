#include "SettingsWindow.h"
#include "../managers/AppearanceManager.h" 

#include <QVBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QSettings>

SettingsWindow::SettingsWindow(QWidget *parent) : QDialog(parent)
{
    //заголовок окна и имя объекта
    setWindowTitle("Настройки");
    setObjectName("SettingsWindow");
    
    //установка модального поведения
    setModal(true);
    
    //фиксированный размер
    setMinimumWidth(300);
    
    //служебные методы создания
    createWidgets();
    
    //загрузка настроек
    loadSettings();
}

void SettingsWindow::createWidgets()
{
    //вертикальный шаблон
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    //форма настроек
    QFormLayout *formLayout = new QFormLayout();
    
    //настройка темы
    m_themeCombo = new QComboBox(this);
    m_themeCombo->addItem("Тёмная");
    m_themeCombo->addItem("Светлая");
    
    // Добавляем строку в форму: Текст "Тема интерфейса:" и сам комбобокс
    formLayout->addRow("Тема интерфейса:", m_themeCombo);
    
    // 2. Настройка автозагрузки (для примера)
    m_autoLoadCheck = new QCheckBox("Загружать последний проект", this);
    formLayout->addRow("", m_autoLoadCheck); // Пустая метка, просто чекбокс
    
    // Добавляем форму в главный вертикальный слой
    mainLayout->addLayout(formLayout);
    
    // Добавляем пружину (stretch), которая будет толкать кнопки вниз,
    // если окно растянут по высоте.
    mainLayout->addStretch();
    
    // -- Кнопки управления (OK / Cancel) --
    // QDialogButtonBox сам знает, как расположить кнопки ОК и Отмена 
    // в зависимости от ОС (на Windows ОК слева, на Mac — справа).
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, // Какие кнопки нужны
        Qt::Horizontal, 
        this
    );
    
    // Добавляем панель с кнопками в самый низ
    mainLayout->addWidget(buttonBox);
    
    // -- Подключение сигналов (Wiring) --
    
    // Если нажали "OK" (accepted) -> вызываем saveSettings()
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsWindow::saveSettings);
    
    // Если нажали "Cancel" (rejected) -> вызываем close() (просто закрыть)
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SettingsWindow::close);
}

// Метод загрузки настроек при открытии окна
void SettingsWindow::loadSettings()
{
    // QSettings — это "волшебная коробка". 
    // На Windows она читает реестр, на Linux/macOS — конфиг-файлы.
    // "Avionix" — имя организации, "Designer" — имя приложения.
    // Они нужны, чтобы знать, где именно искать наши настройки.
    QSettings settings("Avionix", "Designer");
    
    // Читаем значение по ключу "theme". 
    // Если ключа нет (первый запуск), вернем 0 (темная тема по умолчанию).
    int themeIndex = settings.value("theme", 0).toInt();
    
    // Устанавливаем это значение в выпадающий список
    m_themeCombo->setCurrentIndex(themeIndex);
    
    // Читаем чекбокс (по умолчанию false)
    bool autoLoad = settings.value("autoLoad", false).toBool();
    m_autoLoadCheck->setChecked(autoLoad);
}

// Метод сохранения настроек (вызывается по кнопке ОК)
void SettingsWindow::saveSettings()
{
    // Снова открываем доступ к хранилищу настроек
    QSettings settings("Avionix", "Designer");
    
    // Записываем текущий выбранный индекс из комбобокса
    settings.setValue("theme", m_themeCombo->currentIndex());
    
    // Записываем состояние чекбокса
    settings.setValue("autoLoad", m_autoLoadCheck->isChecked());
    
    // -- Применение изменений немедленно --
    
    // Получаем менеджер внешнего вида
    auto am = AppearanceManager::instance();
    
    // Смотрим, что выбрал пользователь
    if (m_themeCombo->currentIndex() == 0) {
        am->applyDarkTheme();  // Включаем тёмную
    } else {
        am->applyLightTheme(); // Включаем светлую
    }
    
    // accept() делает две вещи:
    // 1. Закрывает окно.
    // 2. Возвращает код результата QDialog::Accepted (если кто-то ждет ответа).
    accept(); 
}
