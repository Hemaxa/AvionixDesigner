/**
 * @file StaticGroupObject.h
 * @brief Статическая группа объектов с несколькими состояниями
 * 
 * Каждое состояние задаётся отдельной init-строкой и содержит
 * свои координаты, размеры, цвет и адрес в блочной памяти.
 * Маска (растровые данные) общая для всех состояний.
 */

#pragma once

#include "BaseObject.h"
#include <QImage>

/**
 * @struct GroupState
 * @brief Одно состояние статической группы
 */
struct GroupState
{
    int x = 0;          // Пиксельная координата X левого верхнего угла
    int y = 0;          // Пиксельная координата Y левого верхнего угла
    int w = 0;          // Ширина габаритного прямоугольника
    int h = 0;          // Высота габаритного прямоугольника
    int addr = 0;       // Адрес смещения в блочной памяти
    QColor color;       // Цвет
    bool enabled = false; // Разрешение видимости
};

/**
 * @class StaticGroupObject
 * @brief Группа со статическими состояниями (до 8)
 */
class StaticGroupObject : public BaseObject
{
    Q_OBJECT
    
public:
    QList<GroupState> states;     // Список состояний
    int groupNumber = 0;         // Номер группы (атрибут nomber)
    int activeState = 0;         // Текущее активное состояние для отображения
    QImage maskImage;            // Растровая маска (общая для всех состояний)
    
    explicit StaticGroupObject(QObject *parent = nullptr);
    
    // Парсит параметры первого состояния из HEX-строки
    void parse(const QString &hexInit, const ParamSchema &schema) override;
    
    // Парсит дополнительные данные: остальные init-строки, маску, атрибут nomber
    void parseExtraData(const QDomElement &element) override;
    
    // Отрисовывает группу
    void draw(QPainter &painter) override;
    
    // Возвращает имя типа
    QString getTypeName() const override;
    
    QList<QPair<QString, QString>> getProperties() const override;
    
    // Устанавливает свойство по имени
    bool setObjectProperty(const QString &name, const QString &value) override;
    
    // Возвращает ограничивающий прямоугольник
    QRectF getBoundingRect() const override;

private:
    // Парсит одно состояние из HEX-строки по схеме
    GroupState parseState(const QString &hexInit, const ParamSchema &schema);
    
    // Схема параметров (сохраняется при первом вызове parse)
    ParamSchema m_schema;
};
