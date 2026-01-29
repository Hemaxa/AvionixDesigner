#ifndef LOGWIDGET_H
#define LOGWIDGET_H

#include <QTextEdit>

/**
 * @brief Виджет для отображения лога
 */
class LogWidget : public QTextEdit
{
    Q_OBJECT

public:
    explicit LogWidget(QWidget *parent = nullptr);

    // Добавить сообщение в лог
    void log(const QString &message);
    
    // Очистить лог
    void clearLog();
};

#endif // LOGWIDGET_H
