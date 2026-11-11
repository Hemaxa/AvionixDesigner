// SettingsWindow - окно настроек приложения.

#include "SettingsWindow.h"
#include "AppearanceManager.h"
#include "ProjectManager.h"
#include "FpgaSchemaRegistry.h"
#include "ExpressionSpinBox.h"

#include <QKeySequence>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QIcon>
#include <QPushButton>
#include <QLabel>
#include "ApplicationSettings.h"
#include <QListWidget>
#include <QScrollArea>
#include <QStackedWidget>
#include <QSpinBox>
#include <QColorDialog>
#include <QPainter>
#include <QPixmap>
#include <QShowEvent>

SettingsWindow::SettingsWindow(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Настройки");
    setObjectName("SettingsWindow");
    setModal(true);
    setMinimumSize(720, 480);
    resize(820, 600);

    createWidgets();
    loadSettings();
}

void SettingsWindow::createWidgets()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 22, 24, 20);
    mainLayout->setSpacing(18);
    auto *contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(18);

    m_listWidget = new QListWidget(this);
    m_listWidget->setFixedWidth(180);
    m_listWidget->addItem("Настройки");
    m_listWidget->addItem("Горячие клавиши");

    m_stackedWidget = new QStackedWidget(this);

    // Вкладка "Настройки"
    auto *settingsWidget = new QWidget(this);
    auto *settingsLayout = new QVBoxLayout(settingsWidget);
    settingsLayout->setContentsMargins(0, 0, 0, 0);
    settingsLayout->setSpacing(18);

    auto *generalGroup = new QGroupBox(QStringLiteral("Общие"), this);
    auto *generalLayout = new QFormLayout(generalGroup);
    generalLayout->setContentsMargins(18, 20, 18, 18);
    generalLayout->setHorizontalSpacing(18);
    generalLayout->setVerticalSpacing(14);
    generalLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    generalLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    generalLayout->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_autoLoadCheck = new QCheckBox("Загружать последний проект", this);
    generalLayout->addRow(m_autoLoadCheck);
    m_fastMoveStepSpin = new ExpressionSpinBox(this);
    m_fastMoveStepSpin->setObjectName("FastMoveStepSpin");
    m_fastMoveStepSpin->setRange(1, 10000);
    m_fastMoveStepSpin->setSuffix(QStringLiteral(" пикселей"));
    generalLayout->addRow(QStringLiteral("Шаг Shift + стрелки:"), m_fastMoveStepSpin);
    settingsLayout->addWidget(generalGroup);

    auto *fpgaGroup = new QGroupBox(QStringLiteral("ПЛИС — STATICGROUP"), this);
    auto *fpgaLayout = new QFormLayout(fpgaGroup);
    fpgaLayout->setRowWrapPolicy(QFormLayout::WrapLongRows);
    m_staticGroupPixelsSpin = new ExpressionSpinBox(this);
    m_staticGroupPixelsSpin->setObjectName("StaticGroupPixelsSpin");
    m_staticGroupPixelsSpin->setRange(1, 65536);
    m_staticGroupPixelsSpin->setSuffix(QStringLiteral(" пикселей"));
    fpgaLayout->addRow(QStringLiteral("Ёмкость памяти:"), m_staticGroupPixelsSpin);
    m_staticGroupElementsSpin = new ExpressionSpinBox(this);
    m_staticGroupElementsSpin->setObjectName("StaticGroupElementsSpin");
    m_staticGroupElementsSpin->setRange(1, 65536);
    fpgaLayout->addRow(QStringLiteral("Количество элементов:"), m_staticGroupElementsSpin);
    settingsLayout->addWidget(fpgaGroup);

    auto *snapGroup = new QGroupBox(QStringLiteral("Сетка и привязки"), this);
    auto *formLayout = new QFormLayout(snapGroup);
    formLayout->setContentsMargins(18, 20, 18, 18);
    formLayout->setHorizontalSpacing(18);
    formLayout->setVerticalSpacing(14);
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    formLayout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    formLayout->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_gridStepSpin = new ExpressionSpinBox(this);
    m_gridStepSpin->setObjectName(QStringLiteral("GridStepSpin"));
    m_gridStepSpin->setRange(1, 1000);
    m_gridStepSpin->setMinimumWidth(160);
    formLayout->addRow("Шаг сетки:", m_gridStepSpin);

    m_gridColorButton = new QPushButton(this);
    m_gridColorButton->setObjectName("ColorSwatchButton");
    m_gridColorButton->setToolTip(tr("Цвет сетки (Alt+G)"));
    m_gridColorButton->setShortcut(QKeySequence("Alt+G"));
    connect(m_gridColorButton, &QPushButton::clicked, this, [this]() {
        chooseColor(&m_currentGridColor, m_gridColorButton, QStringLiteral("Цвет сетки"));
    });
    formLayout->addRow("Цвет сетки:", m_gridColorButton);

    m_snapCanvasColorButton = new QPushButton(this);
    m_snapCanvasColorButton->setObjectName("ColorSwatchButton");
    m_snapCanvasColorButton->setToolTip(tr("Цвет линий границ (Alt+C)"));
    m_snapCanvasColorButton->setShortcut(QKeySequence("Alt+C"));
    connect(m_snapCanvasColorButton, &QPushButton::clicked, this, [this]() {
        chooseColor(&m_currentSnapCanvasColor, m_snapCanvasColorButton, QStringLiteral("Цвет привязки к границам"));
    });
    formLayout->addRow("Линии границ:", m_snapCanvasColorButton);

    m_snapGridColorButton = new QPushButton(this);
    m_snapGridColorButton->setObjectName("ColorSwatchButton");
    m_snapGridColorButton->setToolTip(tr("Цвет линий сетки (Alt+S)"));
    m_snapGridColorButton->setShortcut(QKeySequence("Alt+S"));
    connect(m_snapGridColorButton, &QPushButton::clicked, this, [this]() {
        chooseColor(&m_currentSnapGridColor, m_snapGridColorButton, QStringLiteral("Цвет привязки к сетке"));
    });
    formLayout->addRow("Линии сетки:", m_snapGridColorButton);

    m_snapObjectColorButton = new QPushButton(this);
    m_snapObjectColorButton->setObjectName("ColorSwatchButton");
    m_snapObjectColorButton->setToolTip(tr("Цвет линий объектов (Alt+O)"));
    m_snapObjectColorButton->setShortcut(QKeySequence("Alt+O"));
    connect(m_snapObjectColorButton, &QPushButton::clicked, this, [this]() {
        chooseColor(&m_currentSnapObjectColor, m_snapObjectColorButton, QStringLiteral("Цвет привязки к объектам"));
    });
    formLayout->addRow("Линии объектов:", m_snapObjectColorButton);

    settingsLayout->addWidget(snapGroup);
    settingsLayout->addStretch();
    auto *settingsScroll = new QScrollArea(this);
    settingsScroll->setWidgetResizable(true);
    settingsScroll->setFrameShape(QFrame::NoFrame);
    settingsScroll->setWidget(settingsWidget);
    m_stackedWidget->addWidget(settingsScroll);

    // Вкладка "Горячие клавиши"
    auto *hotkeysScroll = new QScrollArea(this);
    hotkeysScroll->setObjectName("HotkeysScrollArea");
    hotkeysScroll->setWidgetResizable(true);
    hotkeysScroll->setFrameShape(QFrame::NoFrame);

    auto *hotkeysWidget = new QWidget(hotkeysScroll);
    auto *hotkeysLayout = new QFormLayout(hotkeysWidget);
    hotkeysLayout->setContentsMargins(0, 0, 0, 0);
    hotkeysLayout->setHorizontalSpacing(22);
    hotkeysLayout->setVerticalSpacing(12);
    hotkeysLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    hotkeysLayout->setRowWrapPolicy(QFormLayout::WrapLongRows);

    hotkeysLayout->addRow(new QLabel("<b>Файл:</b>", this));
    hotkeysLayout->addRow("Создать проект:", new QLabel(QKeySequence(QKeySequence::New).toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Открыть проект:", new QLabel(QKeySequence(QKeySequence::Open).toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Сохранить проект:", new QLabel(QKeySequence(QKeySequence::Save).toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Сохранить как:", new QLabel(QKeySequence(QKeySequence::SaveAs).toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Выход:", new QLabel(QKeySequence(QKeySequence::Quit).toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Настройки приложения:", new QLabel(QKeySequence("Ctrl+,").toString(QKeySequence::NativeText), this));

    hotkeysLayout->addRow(new QLabel("<br><b>Редактирование:</b>", this));
    hotkeysLayout->addRow("Отменить:", new QLabel(QKeySequence(QKeySequence::Undo).toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Повторить:", new QLabel(QKeySequence(QKeySequence::Redo).toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Копировать:", new QLabel(QKeySequence(QKeySequence::Copy).toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Вставить:", new QLabel(QKeySequence(QKeySequence::Paste).toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Сдвинуть выделение на 1 пиксель:", new QLabel("← / ↑ / → / ↓", this));
    hotkeysLayout->addRow("Сдвинуть с настраиваемым шагом:", new QLabel("Shift + ← / ↑ / → / ↓", this));
    hotkeysLayout->addRow("Удалить объект:", new QLabel(QKeySequence(QKeySequence::Delete).toString(QKeySequence::NativeText)
        + " / " + QKeySequence(Qt::Key_Backspace).toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Сгруппировать:", new QLabel(QKeySequence("Ctrl+G").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Разгруппировать:", new QLabel(QKeySequence("Ctrl+Shift+G").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Объединить в staticgroup:", new QLabel(QKeySequence("Ctrl+Alt+G").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Изменить прозрачность:", new QLabel(QKeySequence("Ctrl+Alt+T").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Сохранять пропорции (вкл/выкл):", new QLabel(QKeySequence("Ctrl+Shift+P").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Поворот с шагом 15° (вкл/выкл):", new QLabel(QKeySequence("Ctrl+Shift+R").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Пропорции при перетаскивании ручки:", new QLabel("Shift + мышь", this));
    hotkeysLayout->addRow("Поворот с шагом 15° / 90°:", new QLabel("Shift / Ctrl + мышь", this));
    hotkeysLayout->addRow("На передний план:", new QLabel(QKeySequence("Ctrl+]").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("На задний план:", new QLabel(QKeySequence("Ctrl+[").toString(QKeySequence::NativeText), this));

    hotkeysLayout->addRow(new QLabel("<br><b>Выравнивание:</b>", this));
    hotkeysLayout->addRow("По левому краю:", new QLabel(QKeySequence("Ctrl+Alt+Left").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("По центру горизонтально:", new QLabel(QKeySequence("Ctrl+Alt+H").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("По правому краю:", new QLabel(QKeySequence("Ctrl+Alt+Right").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("По верхнему краю:", new QLabel(QKeySequence("Ctrl+Alt+Up").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("По центру вертикально:", new QLabel(QKeySequence("Ctrl+Alt+V").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("По нижнему краю:", new QLabel(QKeySequence("Ctrl+Alt+Down").toString(QKeySequence::NativeText), this));

    hotkeysLayout->addRow(new QLabel("<br><b>Вид и привязки:</b>", this));
    hotkeysLayout->addRow("Сбросить масштаб:", new QLabel(QKeySequence("Ctrl+0").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Показать сетку:", new QLabel(QKeySequence("Shift+V").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Привязка к сетке:", new QLabel(QKeySequence("Shift+G").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Привязка к экрану:", new QLabel(QKeySequence("Shift+C").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Привязка к объектам:", new QLabel(QKeySequence("Shift+O").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Цвет фона:", new QLabel(QKeySequence("Ctrl+Alt+C").toString(QKeySequence::NativeText), this));
    const QStringList panelNames = {"Слои", "Список объектов", "Свойства объекта", "Библиотека объектов",
        "Настройки рабочей области", "Управление ПЛИС", "Симулятор ПЛИС", "Позиционирование самолета"};
    for (int i = 0; i < panelNames.size(); ++i)
        hotkeysLayout->addRow(QStringLiteral("Показать / скрыть: %1").arg(panelNames[i]), new QLabel(QKeySequence(QStringLiteral("Ctrl+Alt+%1").arg(i + 1)).toString(QKeySequence::NativeText), this));

    hotkeysLayout->addRow(new QLabel("<br><b>Слои (фокус в панели слоёв):</b>", this));
    hotkeysLayout->addRow("Добавить слой:", new QLabel(QKeySequence("Ctrl+Shift+N").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Дублировать активный слой:", new QLabel(QKeySequence("Ctrl+Shift+D").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Объединить выделенные слои:", new QLabel(QKeySequence("Ctrl+Shift+M").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Удалить активный слой:", new QLabel(QKeySequence("Ctrl+Shift+Delete").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Цвет выделения активного слоя:", new QLabel(QKeySequence("Ctrl+Shift+C").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Показать все слои:", new QLabel(QKeySequence("Ctrl+Shift+A").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Переименовать слой:", new QLabel(QKeySequence("F2").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Выбрать все слои / диапазон:", new QLabel("Ctrl+A / Shift + ↑ или ↓", this));

    hotkeysLayout->addRow(new QLabel("<br><b>Текущая строка списка объектов:</b>", this));
    hotkeysLayout->addRow("Видимость объекта или группы:", new QLabel(QKeySequence("Ctrl+Shift+H").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Блокировка выделения:", new QLabel(QKeySequence("Ctrl+L").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Включить / выключить экспорт:", new QLabel(QKeySequence("Ctrl+E").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Переименовать объект или группу:", new QLabel(QKeySequence("F2").toString(QKeySequence::NativeText), this));

    hotkeysLayout->addRow(new QLabel("<br><b>Добавление объектов:</b>", this));
    int objectShortcut = 1;
    for (const auto &descriptor : FpgaSchemaRegistry::instance()->editorObjectCatalog())
        if (descriptor.creatableInMenu)
            hotkeysLayout->addRow(descriptor.title, new QLabel(QKeySequence(QStringLiteral("Alt+%1").arg(objectShortcut++)).toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Добавить статическое изображение:", new QLabel(QKeySequence("Ctrl+I").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Добавить вращающееся изображение:", new QLabel(QKeySequence("Ctrl+Shift+I").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Проверить память ПЛИС:", new QLabel(QKeySequence("Ctrl+Alt+M").toString(QKeySequence::NativeText), this));

    hotkeysLayout->addRow(new QLabel("<br><b>ПЛИС:</b>", this));
    hotkeysLayout->addRow("Запустить режимы:", new QLabel(QKeySequence("Ctrl+Return").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Остановить режимы:", new QLabel(QKeySequence("Ctrl+Shift+Return").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("BIN dump:", new QLabel(QKeySequence("Ctrl+Alt+B").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Симулятор ПЛИС:", new QLabel(QKeySequence("Ctrl+Alt+F").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("UART:", new QLabel(QKeySequence("Ctrl+Alt+U").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Экспорт конфигурации ПЛИС:", new QLabel(QKeySequence("Ctrl+Alt+E").toString(QKeySequence::NativeText), this));

    hotkeysLayout->addRow(new QLabel("<br><b>Диалоги:</b>", this));
    hotkeysLayout->addRow("Выбор пути конфигурации:", new QLabel(QKeySequence("Alt+O").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Обновить список UART-портов:", new QLabel(QKeySequence("Alt+R").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Цвет фона нового проекта:", new QLabel(QKeySequence("Alt+C").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Цвет сетки (в настройках):", new QLabel(QKeySequence("Alt+G").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Цвет линий границ:", new QLabel(QKeySequence("Alt+C").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Цвет линий сетки:", new QLabel(QKeySequence("Alt+S").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Цвет линий объектов:", new QLabel(QKeySequence("Alt+O").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Сбросить все настройки:", new QLabel(QKeySequence("Alt+R").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Подтвердить / отменить:", new QLabel("Enter / Esc", this));
    hotkeysLayout->addRow("Переход между полями / нажать кнопку или переключить флажок:", new QLabel("Tab / Shift+Tab; Space", this));

    hotkeysLayout->addRow(new QLabel("<br><b>Позиционирование самолета:</b>", this));
    hotkeysLayout->addRow("Автопилот (вкл/выкл):", new QLabel(QKeySequence("Ctrl+Alt+A").toString(QKeySequence::NativeText), this));
    hotkeysLayout->addRow("Курс ±0,05°:", new QLabel("← / →", this));
    hotkeysLayout->addRow("Курс ±1°:", new QLabel("Shift + ← / →", this));
    hotkeysLayout->addRow("Тангаж ±1 (yscreen):", new QLabel("↑ / ↓", this));
    hotkeysLayout->addRow("Тангаж ±10:", new QLabel("Shift + ↑ / ↓", this));
    hotkeysLayout->addRow("Высота ±0,25 м:", new QLabel("+ / − (также =)", this));
    hotkeysLayout->addRow("Высота ±10 м:", new QLabel("Shift + (+ / − / =)", this));
    hotkeysLayout->addRow("Высота ±100 м (на ползунке):", new QLabel("Page Up / Page Down", this));
    auto *aircraftHint = new QLabel("Клавиши действуют при фокусе в панели позиционирования. "
        "В числовых полях сохраняется обычное редактирование. На основной клавиатуре «=» "
        "даёт точный набор высоты, Shift+= — ускоренный.", this);
    aircraftHint->setWordWrap(true);
    hotkeysLayout->addRow(aircraftHint);

    hotkeysScroll->setWidget(hotkeysWidget);
    m_stackedWidget->addWidget(hotkeysScroll);

    connect(m_listWidget, &QListWidget::currentRowChanged, m_stackedWidget, &QStackedWidget::setCurrentIndex);
    m_listWidget->setCurrentRow(0);

    contentLayout->addWidget(m_listWidget);
    contentLayout->addWidget(m_stackedWidget);

    mainLayout->addLayout(contentLayout);

    auto *bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(12);
    QPushButton *resetButton = new QPushButton("Сбросить все настройки", this);
    resetButton->setObjectName("ResetButton");
    resetButton->setShortcut(QKeySequence("Alt+R"));
    resetButton->setToolTip(tr("Сбросить все настройки (Alt+R)"));
    bottomLayout->addWidget(resetButton);
    bottomLayout->addStretch();

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Отмена"));

    bottomLayout->addWidget(buttonBox);
    mainLayout->addLayout(bottomLayout);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsWindow::saveSettings);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SettingsWindow::close);
    connect(resetButton, &QPushButton::clicked, this, &SettingsWindow::resetAllSettings);
}

void SettingsWindow::chooseColor(QColor *targetColor, QPushButton *button, const QString &title)
{
    if (!targetColor || !button)
        return;

    const QColor selectedColor = QColorDialog::getColor(*targetColor, this, title, QColorDialog::ShowAlphaChannel | QColorDialog::DontUseNativeDialog);
    if (selectedColor.isValid()) {
        *targetColor = selectedColor;
        updateColorButtonIcon(button, selectedColor);
    }
}

void SettingsWindow::updateColorButtonIcon(QPushButton *button, const QColor &color)
{
    if (!button)
        return;

    QPixmap pixmap(30, 22);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(color);
    painter.setPen(QPen(QColor(230, 245, 255, 170), 1));
    painter.drawRoundedRect(QRectF(1, 1, 28, 20), 4, 4);
    painter.end();
    button->setIcon(QIcon(pixmap));
    button->setIconSize(pixmap.size());
}

void SettingsWindow::showEvent(QShowEvent *event)
{
    loadSettings();
    QDialog::showEvent(event);
}

void SettingsWindow::loadSettings()
{
    auto settings = ApplicationSettings::create();
    m_autoLoadCheck->setChecked(settings.value("autoLoad", false).toBool());
    m_fastMoveStepSpin->setValue(ApplicationSettings::fastMoveStep());
    m_staticGroupPixelsSpin->setValue(ApplicationSettings::staticGroupPixels());
    m_staticGroupElementsSpin->setValue(ApplicationSettings::staticGroupElements());

    m_gridStepSpin->setValue(qBound(1, settings.value("gridStep", 10).toInt(), 1000));
    m_currentGridColor = QColor(settings.value("gridColor", "#3778b4c8").toString());
    m_currentSnapCanvasColor = QColor(settings.value("snapCanvasGuideColor", "#d2ff5c7a").toString());
    m_currentSnapGridColor = QColor(settings.value("snapGridGuideColor", "#d256d3ff").toString());
    m_currentSnapObjectColor = QColor(settings.value("snapObjectGuideColor", "#dcffca58").toString());
    auto *project = ProjectManager::instance();
    if (project->getCanvasWidth() > 0 && project->getCanvasHeight() > 0) {
        m_gridStepSpin->setValue(project->gridStep());
        m_currentGridColor = project->gridColor();
        m_currentSnapCanvasColor = project->snapCanvasGuideColor();
        m_currentSnapGridColor = project->snapGridGuideColor();
        m_currentSnapObjectColor = project->snapObjectGuideColor();
    }
    updateColorButtonIcon(m_gridColorButton, m_currentGridColor);
    updateColorButtonIcon(m_snapCanvasColorButton, m_currentSnapCanvasColor);
    updateColorButtonIcon(m_snapGridColorButton, m_currentSnapGridColor);
    updateColorButtonIcon(m_snapObjectColorButton, m_currentSnapObjectColor);
}

void SettingsWindow::saveSettings()
{
    auto settings = ApplicationSettings::create();
    settings.setValue("autoLoad", m_autoLoadCheck->isChecked());
    settings.setValue("editor/fastMoveStep", m_fastMoveStepSpin->value());
    settings.setValue("fpga/staticGroupPixels", m_staticGroupPixelsSpin->value());
    settings.setValue("fpga/staticGroupElements", m_staticGroupElementsSpin->value());
    settings.setValue("gridStep", m_gridStepSpin->value());
    settings.setValue("gridColor", m_currentGridColor.name(QColor::HexArgb));
    settings.setValue("snapCanvasGuideColor", m_currentSnapCanvasColor.name(QColor::HexArgb));
    settings.setValue("snapGridGuideColor", m_currentSnapGridColor.name(QColor::HexArgb));
    settings.setValue("snapObjectGuideColor", m_currentSnapObjectColor.name(QColor::HexArgb));

    AppearanceManager::instance()->applyAvionixTheme();
    ProjectManager::instance()->reloadGlobalSettings();

    accept();
}

void SettingsWindow::resetAllSettings()
{
    auto settings = ApplicationSettings::create();
    settings.clear();

    AppearanceManager::instance()->applyAvionixTheme();
    ProjectManager::instance()->reloadGlobalSettings();
    loadSettings();

    emit settingsReset();
}
