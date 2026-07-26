//FontExportDialog - диалог выбора алфавитов, которые будут зашиты в font-ресурсы XML

#pragma once

#include <QDialog>
#include <QList>
#include <QString>
#include <QStringList>

class QTableWidget;

struct FontExportDialogEntry
{
    QString fontFamily;
    int pixelSize = 14;
    int textCount = 0;
    QString sampleText;
    QStringList alphabetGroups;
};

class FontExportDialog : public QDialog
{
    Q_OBJECT

public:
    //создаёт диалог настройки алфавитов для списка экспортируемых шрифтов
    explicit FontExportDialog(const QList<FontExportDialogEntry> &entries, QWidget *parent = nullptr);

    //возвращает выбранные пользователем алфавиты для каждого шрифта
    QList<FontExportDialogEntry> entries() const;

private:
    //заполняет таблицу строками шрифтов и чекбоксами алфавитов
    void populateTable();

    QList<FontExportDialogEntry> m_entries;
    QTableWidget *m_table = nullptr;
};
