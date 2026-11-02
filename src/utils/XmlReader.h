// XmlReader - чтение целочисленных атрибутов растровых данных XML.

#pragma once

#include <QString>
#include <QDomElement>

// XmlReader - чтение целочисленных атрибутов растровых данных XML.
class XmlReader
{
public:
    // читает целочисленный атрибут
    static int readInt(const QDomElement &element, const QString &name, int defaultValue = 0)
    {
        if (!element.hasAttribute(name))
            return defaultValue;

        bool ok;
        int value = element.attribute(name).toInt(&ok);
        return ok ? value : defaultValue;
    }

};
