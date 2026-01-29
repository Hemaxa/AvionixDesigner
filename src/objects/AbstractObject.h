/**
 * @file AbstractObject.h
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
    int offset;
    int size;
};

/// Тип для схемы параметров
using ParamSchema = QMap<QString, ParamInfo>;

/**
 * @class AbstractObject
 * @brief Базовый класс для всех графических объектов
 */
class AbstractObject : public QObject
{
    Q_OBJECT
    
public:
    explicit AbstractObject(QObject *parent = nullptr) 
        : QObject(parent) 
    {}
    
    virtual ~AbstractObject() = default;
    
    /**
     * @brief Парсит основные параметры из HEX-строки
     */
    virtual void parse(const QString &hexInit, const ParamSchema &schema) = 0;
    
    /**
     * @brief Парсит дополнительные данные из XML-элемента
     */
    virtual void parseExtraData(const QDomElement &element) 
    { 
        Q_UNUSED(element); 
    }
    
    /**
     * @brief Отрисовывает объект
     */
    virtual void draw(QPainter &painter) = 0;
    
    /**
     * @brief Возвращает тип объекта
     */
    virtual QString typeName() const = 0;
    
    /**
     * @brief Возвращает свойства объекта
     */
    virtual QList<QPair<QString, QString>> getProperties() const = 0;
    
    /**
     * @brief Возвращает ограничивающий прямоугольник
     */
    virtual QRectF getBoundingRect() const = 0;

signals:
    void changed();
};
