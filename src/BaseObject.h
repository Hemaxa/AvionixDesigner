//BaseObject - базовый абстрактный класс для всех графических объектов
 
#pragma once

#include <QObject>
#include <QPainter>
#include <QMap>
#include <QString>
#include <QDomElement>

//структура одного конкретного параметра в битовой схеме
struct ParamInfo 
{
    int offset; //смещение в битах
    int size; //размер в битах
};

//тип для схемы параметров
using ParamSchema = QMap<QString, ParamInfo>;

class BaseObject : public QObject
{
    Q_OBJECT
    
public:
    //виртуальные методы должны быть переопределены в классах наследниках
    //объект знает, какие у него должны быть параметры, сам их читает и сам по ним себя отрисовывает

    explicit BaseObject(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~BaseObject() = default;
    
    //виртуальный метод, который парсит основные параметры из HEX-строки
    virtual void parse(const QString &hexInit, const ParamSchema &schema) = 0;
    
    //виртуальный метод, который парсит дополнительные данные из XML-элемента
    virtual void parseExtraData(const QDomElement &element) { Q_UNUSED(element); }
    
    //виртуальный метод, который отрисовывает объект на холсте
    virtual void draw(QPainter &painter) = 0;
    
    //геттеры для свойств объекта
    virtual QString getTypeName() const = 0; //возвращает тип объекта
    virtual QList<QPair<QString, QString>> getProperties() const = 0; //возвращает список свойств объекта
    virtual QRectF getBoundingRect() const = 0;  //возвращает ограничивающий прямоугольник (занимаемое пространство)

signals:
    void changed();  //сигнал об изменении объекта
};
