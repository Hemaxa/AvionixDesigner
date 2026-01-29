/**
 * @file LogWindow.h
 * @brief Окно лога приложения
 */

#pragma once

#include <QTextEdit>

/**
 * @class LogWindow
 * @brief Виджет лога с цветными сообщениями
 */
class LogWindow : public QTextEdit
{
    Q_OBJECT
    
public:
    explicit LogWindow(QWidget *parent = nullptr);
    
public slots:
    void log(const QString &message);
    void logError(const QString &message);
    void logWarning(const QString &message);
    void logSuccess(const QString &message);
    void clearLog();

private:
    void appendMessage(const QString &message, const QColor &color);
};
