//ObjectPropertiesPanel - панель свойств выбранного объекта

#pragma once

#include "BasePanel.h"
#include <QSharedPointer>

class QTableWidget;
class QLabel;
class QScrollArea;
class QStackedWidget;
class BaseObject;

class ObjectPropertiesPanel : public BasePanel
{
    Q_OBJECT
    
public:
    explicit ObjectPropertiesPanel(QWidget *parent = nullptr);
    
public slots:
    //показывает свойства объекта по индексу
    void showObjectProperties(int index);
    
    //показывает свойства переданного объекта
    void showProperties(QSharedPointer<BaseObject> obj);
    
    //очищает панель свойств
    void clearProperties();

signals:
    //сигнал об изменении свойства объекта
    void propertyChanged();

private slots:
    //обработка изменения ячейки таблицы
    void onCellChanged(int row, int column);

private:
    //заполняет таблицу свойствами объекта
    void populateTable(const QList<QPair<QString, QString>> &props);
    
    //очищает таблицу
    void clearTable();
    
    QLabel *m_titleLabel; //заголовок панели
    QLabel *m_subtitleLabel; //подзаголовок панели
    QLabel *m_emptyStateLabel; //заглушка пустого состояния
    QTableWidget *m_tableWidget; //таблица свойств
    QScrollArea *m_scrollArea; //область прокрутки
    QStackedWidget *m_contentStack; //переключатель состояний панели
    QSharedPointer<BaseObject> m_currentObject; //текущий объект
    bool m_updating = false; //флаг программного обновления
};
