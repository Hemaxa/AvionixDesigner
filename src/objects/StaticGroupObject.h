/**
 * @file StaticGroupObject.h
 * @brief Статическая группа объектов (заглушка)
 */

#pragma once

#include "AbstractObject.h"

/**
 * @class StaticGroupObject
 * @brief Группа со статическими состояниями
 */
class StaticGroupObject : public AbstractObject
{
    Q_OBJECT
    
public:
    double x = 0;
    double y = 0;
    double width = 0;
    double height = 0;
    QColor color;
    int address = 0;
    int groupNumber = 0;
    
    explicit StaticGroupObject(QObject *parent = nullptr);
    
    void parse(const QString &hexInit, const ParamSchema &schema) override;
    void draw(QPainter &painter) override;
    QString typeName() const override;
    QList<QPair<QString, QString>> getProperties() const override;
    QRectF getBoundingRect() const override;
};
