/**
 * @file XmlReader.h
 * @brief Вспомогательные функции для безопасной работы с XML
 * 
 * Обертка над QDomDocument и QDomElement для безопасного чтения
 * атрибутов с поддержкой значений по умолчанию.
 */

#pragma once

#include <QString>
#include <QDomElement>
#include <QColor>

/**
 * @class XmlReader
 * @brief Помощник для работы с XML документами
 */
class XmlReader
{
public:
    // Читает целочисленный атрибут
    static int readInt(const QDomElement &element, const QString &name, int defaultValue = 0)
    {
        if (!element.hasAttribute(name)) 
            return defaultValue;
        
        bool ok;
        int value = element.attribute(name).toInt(&ok);
        return ok ? value : defaultValue;
    }
    
    // Читает вещественный атрибут
    static double readDouble(const QDomElement &element, const QString &name, double defaultValue = 0.0)
    {
        if (!element.hasAttribute(name)) 
            return defaultValue;
        
        bool ok;
        double value = element.attribute(name).toDouble(&ok);
        return ok ? value : defaultValue;
    }
    
    // Читает строковый атрибут
    static QString readString(const QDomElement &element, const QString &name, 
                              const QString &defaultValue = QString())
    {
        return element.attribute(name, defaultValue);
    }
    
    // Читает цвет из атрибута в формате "#RGB" или "#RRGGBB"
    static QColor readColor(const QDomElement &element, const QString &name,
                            const QColor &defaultColor = Qt::black)
    {
        if (!element.hasAttribute(name))
            return defaultColor;
        
        QString colorStr = element.attribute(name);
        
        // Поддержка формата "#0" или "#FF00FF"
        if (colorStr.startsWith("#")) {
            bool ok;
            uint colorVal = colorStr.mid(1).toUInt(&ok, 16);
            if (ok) {
                // Если короткий формат (один символ), это индекс палитры
                if (colorStr.length() <= 2) {
                    int gray = colorVal * 17;
                    return QColor(gray, gray, gray);
                }
                // Стандартный RGB формат
                return QColor((colorVal >> 16) & 0xFF, 
                              (colorVal >> 8) & 0xFF, 
                              colorVal & 0xFF);
            }
        }
        
        // Попытка распознать именованный цвет Qt
        QColor color(colorStr);
        return color.isValid() ? color : defaultColor;
    }
    
    // Находит первый дочерний элемент с указанным именем
    static QDomElement findChild(const QDomElement &parent, const QString &tagName)
    {
        return parent.firstChildElement(tagName);
    }
    
    // Читает текстовое содержимое дочернего элемента
    static QString readChildText(const QDomElement &parent, const QString &tagName)
    {
        QDomElement child = parent.firstChildElement(tagName);
        return child.isNull() ? QString() : child.text().trimmed();
    }
};
