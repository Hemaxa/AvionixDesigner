//StaticGroupObject - класс группы статических растровых объектов с несколькими состояниями

#pragma once

#include "BaseObject.h"
#include <QImage>

//одно состояние статической группы
//состояния можно использовать как для разных объектов, так и для изменения вида одного объекта, через вкл/выкл нужных состояний
struct GroupState
{
    int x = 0; //пиксельная координата X левого верхнего угла
    int y = 0; //пиксельная координата Y левого верхнего угла
    int w = 0; //ширина габаритного прямоугольника
    int h = 0; //высота габаритного прямоугольника
    int addr = 0; //адрес смещения в блочной памяти
    QColor color; //цвет
    bool enabled = false; //разрешение видимости (вкл/выкл состояния)
};

class StaticGroupObject : public BaseObject
{
    Q_OBJECT
    
public:
    QList<GroupState> states; //список состояний
    int groupNumber = 0; //номер группы (атрибут nomber)
    QList<QImage> maskImages; //растровые маски (по одной на состояние)
    
    explicit StaticGroupObject(QObject *parent = nullptr);
    
    //переопределение виртуальных методов из базового класса
    void parse(const QString &hexInit, const ParamSchema &schema) override;
    void draw(QPainter &painter) override;
    QString getTypeName() const override;
    QString getDisplayName() const override;
    QList<QPair<QString, QString>> getProperties() const override;
    
    // Взаимодействие
    void moveBy(double dx, double dy) override;
    void resizeBy(int edgeFlags, double dx, double dy) override;
    
    bool setObjectProperty(const QString &name, const QString &value) override;
    void parseExtraData(const QDomElement &element) override;
    QMap<QString, quint32> serializeParams() const override;
    QRectF getBoundingRect() const override;

    //сереализация параметров конкретного состояния
    QMap<QString, quint32> serializeState(int stateIndex) const;

protected:
    void rebuildStateAddresses();

private:
    //парсит одно состояние из HEX-строки по схеме
    GroupState parseState(const QString &hexInit, const ParamSchema &schema);
    
    //схема параметров (сохраняется при первом вызове parse)
    ParamSchema m_schema;
};
