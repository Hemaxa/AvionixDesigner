#include "LogWidget.h"
#include <QDateTime>

LogWidget::LogWidget(QWidget *parent)
    : QTextEdit(parent)
{
    setReadOnly(true);
    setMinimumHeight(100);
    setMaximumHeight(200);
    
    // Моноширинный шрифт для лога
    QFont font("Menlo", 10);
    font.setStyleHint(QFont::Monospace);
    setFont(font);
    
    // Темная тема для лога
    setStyleSheet("QTextEdit { background-color: #1e1e1e; color: #d4d4d4; }");
}

void LogWidget::log(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    append(QString("[%1] %2").arg(timestamp, message));
}

void LogWidget::clearLog()
{
    clear();
}
