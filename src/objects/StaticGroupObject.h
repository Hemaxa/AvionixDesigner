/**
 * @file StaticGroupObject.h
 * @brief Статическая группа объектов
 */

#pragma once

#include "BaseObject.h"

/**
 * @class StaticGroupObject
 * @brief Группа со статическими состояниями
 */
class StaticGroupObject : public BaseObject
{
    Q_OBJECT
    
public:
    double x = 0;         // Координата X
    double y = 0;         // Координата Y
    double width = 0;     // Ширина
    double height = 0;    // Высота
    QColor color;         // Цвет
    int address = 0;      // Адрес в памяти
    int groupNumber = 0;  // Номер группы
    
    explicit StaticGroupObject(QObject *parent = nullptr);
    
    // Парсит параметры из HEX-строки
    void parse(const QString &hexInit, const ParamSchema &schema) override;
    
    // Отрисовывает группу
    void draw(QPainter &painter) override;
    
    // Возвращает имя типа
    QString getTypeName() const override;
    
    QList<QPair<QString, QString>> getProperties() const override;
    
    // Устанавливает свойство по имени
    bool setObjectProperty(const QString &name, const QString &value) override;
    
    // Возвращает ограничивающий прямоугольник
    QRectF getBoundingRect() const override;
};
