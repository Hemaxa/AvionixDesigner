/**
 * @file LogWindow.cpp
 * @brief Реализация окна лога
 */

#include "LogWindow.h"
#include "../managers/ProjectManager.h"
#include "../managers/AppearanceManager.h"
#include <QScrollBar>
#include <QDateTime>

LogWindow::LogWindow(QWidget *parent)
    : QTextEdit(parent)
{
    setReadOnly(true);
    setMinimumHeight(80);
    setMaximumHeight(200);
    
    setFont(AppearanceManager::instance()->getMonoFont());
    
    connect(ProjectManager::instance(), &ProjectManager::logMessage,
            this, &LogWindow::log);
}

void LogWindow::log(const QString &message)
{
    appendMessage(message, QColor(0xd4, 0xd4, 0xd4));
}

void LogWindow::logError(const QString &message)
{
    appendMessage("ОШИБКА: " + message, QColor(0xf4, 0x43, 0x36));
}

void LogWindow::logWarning(const QString &message)
{
    appendMessage("ВНИМАНИЕ: " + message, QColor(0xff, 0x98, 0x00));
}

void LogWindow::logSuccess(const QString &message)
{
    appendMessage(message, QColor(0x4c, 0xaf, 0x50));
}

void LogWindow::clearLog()
{
    clear();
}

void LogWindow::appendMessage(const QString &message, const QColor &color)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    
    QString html = QString("<span style=\"color: #808080;\">[%1]</span> "
                           "<span style=\"color: %2;\">%3</span>")
        .arg(timestamp)
        .arg(color.name())
        .arg(message.toHtmlEscaped());
    
    append(html);
    
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}
