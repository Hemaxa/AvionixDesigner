/**
 * @file BaseObject.h
 * @brief Базовый абстрактный класс для всех графических объектов
 */

#pragma once

#include <QObject>
#include <QPainter>
#include <QMap>
#include <QString>
#include <QDomElement>

/**
 * @struct ParamInfo
 * @brief Информация о параметре в битовой схеме
 */
struct ParamInfo 
{
    int offset;  // Смещение в битах
    int size;    // Размер в битах
};

// Тип для схемы параметров
using ParamSchema = QMap<QString, ParamInfo>;

/**
 * @class BaseObject
 * @brief Базовый класс для всех графических объектов сцены
 */
class BaseObject : public QObject
{
    Q_OBJECT
    
public:
    explicit BaseObject(QObject *parent = nullptr) 
        : QObject(parent) 
    {}
    
    virtual ~BaseObject() = default;
    
    // Парсит основные параметры из HEX-строки
    virtual void parse(const QString &hexInit, const ParamSchema &schema) = 0;
    
    // Парсит дополнительные данные из XML-элемента
    virtual void parseExtraData(const QDomElement &element) 
    { 
        Q_UNUSED(element); 
    }
    
    // Отрисовывает объект на холсте
    virtual void draw(QPainter &painter) = 0;
    
    // Возвращает тип объекта
    virtual QString typeName() const = 0;
    
    // Возвращает список свойств объекта
    virtual QList<QPair<QString, QString>> getProperties() const = 0;
    
    // Возвращает ограничивающий прямоугольник
    virtual QRectF getBoundingRect() const = 0;

signals:
    void changed();  // Сигнал об изменении объекта
};
