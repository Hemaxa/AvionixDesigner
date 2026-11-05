// ObjectPropertiesPanel - панель свойств выбранного объекта.

#pragma once

#include "BasePanel.h"
#include <QSharedPointer>
#include <QSet>

class QTableWidget;
class QLabel;
class QScrollArea;
class QStackedWidget;
class BaseObject;

// ObjectPropertiesPanel - панель свойств выбранного объекта.
class ObjectPropertiesPanel : public BasePanel
{
    Q_OBJECT

public:
    explicit ObjectPropertiesPanel(QWidget *parent = nullptr);

public slots:
    // показывает свойства объекта по индексу
    void showObjectProperties(int index);
    void showSelectedObjects(const QList<int> &indexes);

    // показывает свойства переданного объекта
    void showProperties(QSharedPointer<BaseObject> obj);

    // очищает панель свойств
    void clearProperties();
    void focusTransparency();

signals:
    // сигнал об изменении свойства объекта
    void propertyChanged();

private slots:
    // обработка изменения ячейки таблицы
    void onCellChanged(int row, int column);
    void onCellDoubleClicked(int row, int column);

private:
    // заполняет таблицу свойствами объекта
    void populateTable(const QList<QPair<QString, QString>> &props);

    // очищает таблицу
    void clearTable();
    void refreshTableValues();
    void refreshHardwareWarnings();
    QWidget *createTransparencyEditor(double value);
    QList<QPair<QString, QString>> commonProperties() const;
    bool applyProperty(const QString &name, const QString &value);
    void updateValueItem(int row, const QString &name, const QString &value);
    void applyTransparency(double value, bool interactive);
    void resizePropertyRows();
    bool isColorProperty(const QString &name) const;
    bool isReadOnlyProperty(const QString &name) const;

    QLabel *m_titleLabel; //заголовок панели
    QLabel *m_subtitleLabel; //подзаголовок панели
    QLabel *m_emptyStateLabel; //заглушка пустого состояния
    QTableWidget *m_tableWidget; //таблица свойств
    QScrollArea *m_scrollArea; //область прокрутки
    QStackedWidget *m_contentStack; //переключатель состояний панели
    QSharedPointer<BaseObject> m_currentObject; //текущий объект
    QSet<QString> m_readOnlyProperties;
    mutable QSet<QString> m_mixedProperties;
    bool m_updating = false; //флаг программного обновления
    bool m_transparencyDragging = false;
    QList<QSharedPointer<BaseObject>> m_currentObjects;
    QList<QMetaObject::Connection> m_objectConnections;
};
