// AppearanceManager - менеджер внешнего вида приложения.

#pragma once

#include <QObject>
#include <QColor>
#include <QFont>
#include <QHash>
#include <QIcon>
#include <QPen>
#include <QSharedPointer>
#include <QTemporaryFile>

// AppearanceManager - менеджер внешнего вида приложения.
class AppearanceManager : public QObject
{
    Q_OBJECT

public:
    // получение единственного экземпляра
    static AppearanceManager* instance();

    // загружает стили из файла
    bool loadStyleSheet(const QString &filePath);

    // применяет тему Avionix Designer
    void applyAvionixTheme();

    // Именованные значения из AvionixDesignerTheme.qss для ручной отрисовки.
    QString value(const QString &name) const;
    QColor color(const QString &name) const;
    double number(const QString &name) const;
    QFont font(const QString &name, const QFont &base, double scale = 1.0) const;
    QPen pen(const QString &name, const QColor &color, double scale = 1.0) const;
    QIcon colorSwatch(const QString &name, const QColor &color) const;

signals:
    void styleChanged();  //сигнал смены стиля

private:
    AppearanceManager();
    QHash<QString, QString> m_values;
    QList<QSharedPointer<QTemporaryFile>> m_themeImages;
};
