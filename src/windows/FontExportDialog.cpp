// FontExportDialog - диалог выбора алфавитов, которые будут зашиты в font-ресурсы XML.

#include "FontExportDialog.h"

#include <QDialogButtonBox>
#include <QCheckBox>
#include <QHeaderView>
#include <QHash>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyleOptionHeader>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {
// AlphabetColumnToggle — смешанное состояние показывается, а нажатие выбирает все или ничего.
class AlphabetColumnToggle : public QCheckBox
{
public:
    using QCheckBox::QCheckBox;
protected:
    void nextCheckState() override
    {
        setCheckState(checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
    }
};

// AlphabetHeaderView — переключатели остаются над своими столбцами при прокрутке и изменении ширины.
class AlphabetHeaderView : public QHeaderView
{
public:
    explicit AlphabetHeaderView(QWidget *parent) : QHeaderView(Qt::Horizontal, parent)
    {
        connect(this, &QHeaderView::geometriesChanged, this, &AlphabetHeaderView::positionToggles);
        connect(this, &QHeaderView::sectionResized, this, &AlphabetHeaderView::positionToggles);
        connect(this, &QHeaderView::sectionMoved, this, &AlphabetHeaderView::positionToggles);
    }

    QCheckBox *addToggle(int section, const QString &title)
    {
        auto *toggle = new AlphabetColumnToggle(title, viewport());
        toggle->setObjectName("FontAlphabetColumnToggle");
        toggle->setTristate(true);
        m_toggles.insert(section, toggle);
        return toggle;
    }

    void positionToggles()
    {
        for (auto it = m_toggles.cbegin(); it != m_toggles.cend(); ++it) {
            const int section = it.key();
            auto *toggle = it.value();
            const QRect area(sectionViewportPosition(section), 0, sectionSize(section), viewport()->height());
            const QSize size = toggle->sizeHint();
            toggle->setGeometry(QRect(QPoint(area.left() + (area.width() - size.width()) / 2,
                                             area.top() + (area.height() - size.height()) / 2), size));
            toggle->setVisible(!isSectionHidden(section) && area.intersects(viewport()->rect()));
        }
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QHeaderView::resizeEvent(event);
        positionToggles();
    }

    QSize sectionSizeFromContents(int section) const override
    {
        const QSize base = QHeaderView::sectionSizeFromContents(section);
        if (auto *toggle = m_toggles.value(section))
            return base.expandedTo(toggle->sizeHint());
        return base;
    }

    void paintSection(QPainter *painter, const QRect &rect, int section) const override
    {
        if (!m_toggles.contains(section)) {
            QHeaderView::paintSection(painter, rect, section);
            return;
        }
        QStyleOptionHeader option;
        initStyleOptionForIndex(&option, section);
        option.rect = rect;
        option.text.clear();
        painter->save();
        style()->drawControl(QStyle::CE_Header, &option, painter, this);
        painter->restore();
    }

private:
    QHash<int, QCheckBox *> m_toggles;
};

// AlphabetColumn - столбец выбора набора символов при экспорте шрифта.
struct AlphabetColumn
{
    QString id;
    QString title;
    QString toolTip;
};

const QList<AlphabetColumn> alphabetColumns()
{
    return {
        {QStringLiteral("digits"), QStringLiteral("0-9"), QStringLiteral("Цифры")},
        {QStringLiteral("latin_upper"), QStringLiteral("A-Z"), QStringLiteral("Латинские прописные буквы")},
        {QStringLiteral("latin_lower"), QStringLiteral("a-z"), QStringLiteral("Латинские строчные буквы")},
        {QStringLiteral("cyrillic_upper"), QStringLiteral("А-Я"), QStringLiteral("Кириллические прописные буквы")},
        {QStringLiteral("cyrillic_lower"), QStringLiteral("а-я"), QStringLiteral("Кириллические строчные буквы")},
        {QStringLiteral("symbols"), QStringLiteral("Символы"), QStringLiteral("α, °, +, -, /")}
    };
}
}

FontExportDialog::FontExportDialog(const QList<FontExportDialogEntry> &entries, QWidget *parent)
    : QDialog(parent)
    , m_entries(entries)
{
    setWindowTitle(QStringLiteral("Алфавиты шрифтов для XML"));
    setObjectName(QStringLiteral("FontExportDialog"));
    setModal(true);
    setMinimumSize(640, 420);
    resize(900, 560);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 22, 24, 20);
    layout->setSpacing(16);

    auto *title = new QLabel(QStringLiteral("Выбор алфавитов для зашивки в XML"), this);
    title->setObjectName(QStringLiteral("DialogTitleLabel"));
    layout->addWidget(title);

    auto *description = new QLabel(
        QStringLiteral("Для каждого нового шрифта выберите полные наборы символов, которые должны получить init и data. "
                       "Символы, уже использованные в строках, будут добавлены обязательно. "
                       "Переключатели в заголовках включают или отключают столбец для всех шрифтов."),
        this
    );
    description->setObjectName(QStringLiteral("DialogDescriptionLabel"));
    description->setWordWrap(true);
    layout->addWidget(description);

    m_table = new QTableWidget(this);
    m_table->setObjectName(QStringLiteral("FontExportTable"));
    m_table->setColumnCount(3 + alphabetColumns().size());
    m_table->setRowCount(m_entries.size());
    auto *header = new AlphabetHeaderView(m_table);
    m_table->setHorizontalHeader(header);
    const auto columns = alphabetColumns();
    for (int column = 0; column < columns.size(); ++column) {
        const auto &alphabet = columns[column];
        auto *toggle = header->addToggle(column + 3, alphabet.title);
        toggle->setProperty("alphabetGroup", alphabet.id);
        toggle->setAccessibleName(alphabet.toolTip + QStringLiteral(" — для всех шрифтов"));
        m_columnToggles.append(toggle);
        connect(toggle, &QCheckBox::clicked, this, [this, toggle, column]() {
            setAlphabetColumn(column + 3, toggle->checkState() == Qt::Checked);
        });
    }
    connect(m_table->horizontalScrollBar(), &QScrollBar::valueChanged, header,
            &AlphabetHeaderView::positionToggles, Qt::QueuedConnection);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(false);
    // На узком окне столбцы доступны через прокрутку, а имя шрифта не исчезает.
    m_table->horizontalHeader()->setMinimumSectionSize(70);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_table->setColumnWidth(0, 220);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    for (int column = 3; column < m_table->columnCount(); ++column)
        m_table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);

    QStringList headers = {
        QStringLiteral("Шрифт"),
        QStringLiteral("Размер"),
        QStringLiteral("Строк")
    };
    for (const AlphabetColumn &column : alphabetColumns())
        headers.append(column.title);
    m_table->setHorizontalHeaderLabels(headers);

    populateTable();
    connect(m_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        if (item->column() >= 3) updateColumnToggles();
    });
    updateColumnToggles();
    layout->addWidget(m_table, 1);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Отмена"));
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);
}

QList<FontExportDialogEntry> FontExportDialog::entries() const
{
    QList<FontExportDialogEntry> result = m_entries;
    const QList<AlphabetColumn> columns = alphabetColumns();

    for (int row = 0; row < result.size() && row < m_table->rowCount(); ++row) {
        QStringList groups;
        for (int column = 0; column < columns.size(); ++column) {
            const auto *item = m_table->item(row, column + 3);
            if (item && item->checkState() == Qt::Checked)
                groups.append(columns[column].id);
        }
        result[row].alphabetGroups = groups;
    }

    return result;
}

void FontExportDialog::populateTable()
{
    const QList<AlphabetColumn> columns = alphabetColumns();

    for (int row = 0; row < m_entries.size(); ++row) {
        const FontExportDialogEntry &entry = m_entries[row];

        auto *fontItem = new QTableWidgetItem(entry.fontFamily);
        fontItem->setToolTip(entry.sampleText);
        m_table->setItem(row, 0, fontItem);

        auto *sizeItem = new QTableWidgetItem(QString::number(entry.pixelSize));
        sizeItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 1, sizeItem);

        auto *countItem = new QTableWidgetItem(QString::number(entry.textCount));
        countItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 2, countItem);

        for (int column = 0; column < columns.size(); ++column) {
            auto *item = new QTableWidgetItem();
            item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
            item->setCheckState(entry.alphabetGroups.contains(columns[column].id) ? Qt::Checked : Qt::Unchecked);
            item->setTextAlignment(Qt::AlignCenter);
            item->setToolTip(columns[column].toolTip);
            m_table->setItem(row, column + 3, item);
        }
    }

    m_table->resizeRowsToContents();
    for (int row = 0; row < m_table->rowCount(); ++row)
        m_table->setRowHeight(row, qMax(42, m_table->rowHeight(row)));
}

void FontExportDialog::setAlphabetColumn(int column, bool checked)
{
    {
        const QSignalBlocker blocker(m_table);
        for (int row = 0; row < m_table->rowCount(); ++row)
            if (auto *item = m_table->item(row, column))
                item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    }
    updateColumnToggles();
}

void FontExportDialog::updateColumnToggles()
{
    const auto columns = alphabetColumns();
    const int rows = m_table->rowCount();
    for (int column = 0; column < m_columnToggles.size(); ++column) {
        int checked = 0;
        for (int row = 0; row < rows; ++row)
            if (const auto *item = m_table->item(row, column + 3))
                checked += item->checkState() == Qt::Checked;
        auto *toggle = m_columnToggles[column];
        toggle->setEnabled(rows > 0);
        toggle->setCheckState(checked == 0 ? Qt::Unchecked : checked == rows ? Qt::Checked : Qt::PartiallyChecked);
        toggle->setToolTip(QStringLiteral("%1: выбрано у %2 из %3 шрифтов. Нажмите, чтобы %4 весь столбец.")
                               .arg(columns[column].toolTip).arg(checked).arg(rows)
                               .arg(checked == rows && rows > 0 ? QStringLiteral("отключить") : QStringLiteral("включить")));
    }
}
