#include "FontExportDialog.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {
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
    setMinimumSize(820, 520);
    resize(900, 560);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 22, 24, 20);
    layout->setSpacing(16);

    auto *title = new QLabel(QStringLiteral("Выбор алфавитов для зашивки в XML"), this);
    title->setObjectName(QStringLiteral("DialogTitleLabel"));
    layout->addWidget(title);

    auto *description = new QLabel(
        QStringLiteral("Для каждого нового шрифта выберите полные наборы символов, которые должны получить init и data. "
                       "Символы, уже использованные в строках, будут добавлены обязательно."),
        this
    );
    description->setObjectName(QStringLiteral("DialogDescriptionLabel"));
    description->setWordWrap(true);
    layout->addWidget(description);

    m_table = new QTableWidget(this);
    m_table->setObjectName(QStringLiteral("FontExportTable"));
    m_table->setColumnCount(3 + alphabetColumns().size());
    m_table->setRowCount(m_entries.size());
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
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
    layout->addWidget(m_table, 1);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
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
