/**
 * @file XmlHelper.h
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
 * @class XmlHelper
 * @brief Помощник для работы с XML документами
 * 
 * Предоставляет методы для безопасного чтения атрибутов XML
 * с автоматическим преобразованием типов и значениями по умолчанию.
 */
class XmlHelper
{
public:
    /**
     * @brief Читает целочисленный атрибут
     * @param element XML-элемент
     * @param name    Имя атрибута
     * @param defaultValue Значение по умолчанию
     * @return Значение атрибута или defaultValue
     */
    static int readInt(const QDomElement &element, const QString &name, int defaultValue = 0)
    {
        if (!element.hasAttribute(name)) 
            return defaultValue;
        
        bool ok;
        int value = element.attribute(name).toInt(&ok);
        return ok ? value : defaultValue;
    }
    
    /**
     * @brief Читает вещественный атрибут
     * @param element XML-элемент
     * @param name    Имя атрибута
     * @param defaultValue Значение по умолчанию
     * @return Значение атрибута или defaultValue
     */
    static double readDouble(const QDomElement &element, const QString &name, double defaultValue = 0.0)
    {
        if (!element.hasAttribute(name)) 
            return defaultValue;
        
        bool ok;
        double value = element.attribute(name).toDouble(&ok);
        return ok ? value : defaultValue;
    }
    
    /**
     * @brief Читает строковый атрибут
     * @param element XML-элемент
     * @param name    Имя атрибута
     * @param defaultValue Значение по умолчанию
     * @return Значение атрибута или defaultValue
     */
    static QString readString(const QDomElement &element, const QString &name, 
                              const QString &defaultValue = QString())
    {
        return element.attribute(name, defaultValue);
    }
    
    /**
     * @brief Читает цвет из атрибута в формате "#RGB" или "#RRGGBB"
     * @param element XML-элемент
     * @param name    Имя атрибута
     * @param defaultColor Цвет по умолчанию
     * @return Распознанный цвет или defaultColor
     */
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
                // Если короткий формат (один символ), это индекс палитры или базовый цвет
                if (colorStr.length() <= 2) {
                    // Черный для #0, белый для #F и т.д.
                    int gray = colorVal * 17; // 0->0, F->255
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
    
    /**
     * @brief Находит первый дочерний элемент с указанным именем
     * @param parent Родительский элемент
     * @param tagName Имя тега
     * @return Найденный элемент или null-элемент
     */
    static QDomElement findChild(const QDomElement &parent, const QString &tagName)
    {
        return parent.firstChildElement(tagName);
    }
    
    /**
     * @brief Читает текстовое содержимое дочернего элемента
     * @param parent Родительский элемент
     * @param tagName Имя дочернего тега
     * @return Текст внутри тега или пустая строка
     */
    static QString readChildText(const QDomElement &parent, const QString &tagName)
    {
        QDomElement child = parent.firstChildElement(tagName);
        return child.isNull() ? QString() : child.text().trimmed();
    }
};
