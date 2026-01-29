/**
 * @file BitParser.h
 * @brief Утилиты для парсинга битовых данных из HEX-строк
 * 
 * Этот класс содержит статические методы для извлечения значений
 * из HEX-строк по битовому смещению и размеру. Используется для
 * декодирования бинарных данных объектов из XML.
 */

#pragma once

#include <QString>
#include <QColor>

/**
 * @class BitParser
 * @brief Парсер битовых данных из HEX-строк
 * 
 * Класс предоставляет методы для:
 * - Извлечения числовых значений из HEX-строки по битовой маске
 * - Конвертации BGR цветов в QColor
 * 
 * Пример использования:
 * @code
 * QString hex = "00FF00";
 * quint32 value = BitParser::extract(hex, 0, 8); // Извлечь 8 бит с позиции 0
 * @endcode
 */
class BitParser 
{
public:
    /**
     * @brief Извлекает числовое значение из HEX-строки
     * 
     * Алгоритм работает с битами, где последний символ строки
     * содержит младшие биты (биты 0-3).
     * 
     * @param hexString HEX-строка (например, "FF00AA")
     * @param offset    Битовое смещение от начала (младших бит)
     * @param size      Количество бит для извлечения
     * @return Извлеченное значение как 32-битное число
     */
    static quint32 extract(const QString &hexString, int offset, int size) 
    {
        quint32 result = 0;
        int len = hexString.length();
        
        // Проходим по каждому биту, который нужно извлечь
        for (int i = 0; i < size; ++i) {
            int targetBitIndex = offset + i;
            
            // Вычисляем индекс символа в строке (с конца)
            // Последний символ содержит биты 0-3
            int charIndex = len - 1 - (targetBitIndex / 4);
            
            if (charIndex < 0) break; // Защита от выхода за границы

            // Преобразуем HEX-символ в число (0-15)
            QChar ch = hexString[charIndex];
            int val = 0;
            if (ch >= '0' && ch <= '9') 
                val = ch.toLatin1() - '0';
            else if (ch >= 'A' && ch <= 'F') 
                val = ch.toLatin1() - 'A' + 10;
            else if (ch >= 'a' && ch <= 'f') 
                val = ch.toLatin1() - 'a' + 10;

            // Определяем, какой бит внутри символа нам нужен (0..3)
            int bitInChar = targetBitIndex % 4;
            
            // Если бит установлен, добавляем его в результат
            if ((val >> bitInChar) & 1) {
                result |= (1 << i);
            }
        }
        return result;
    }
    
    /**
     * @brief Конвертирует BGR значение в QColor
     * 
     * В формате Windows/CorelDRAW цвета хранятся как BGR (Blue-Green-Red).
     * Этот метод конвертирует их в стандартный RGB формат Qt.
     * 
     * @param bgrValue 24-битное BGR значение
     * @return QColor в формате RGB
     */
    static QColor parseColor(quint32 bgrValue) 
    {
        int r = bgrValue & 0xFF;           // Красный в младших битах
        int g = (bgrValue >> 8) & 0xFF;    // Зеленый в средних
        int b = (bgrValue >> 16) & 0xFF;   // Синий в старших
        return QColor(r, g, b);
    }
    
    /**
     * @brief Извлекает знаковое целое из HEX-строки
     * 
     * Используется для значений sin/cos в RotationObject,
     * где требуется знаковая интерпретация битов.
     * 
     * @param hexString HEX-строка
     * @param offset    Битовое смещение
     * @param size      Количество бит
     * @return Знаковое целое значение
     */
    static qint32 extractSigned(const QString &hexString, int offset, int size)
    {
        quint32 raw = extract(hexString, offset, size);
        
        // Проверяем старший бит для определения знака
        if (raw & (1 << (size - 1))) {
            // Отрицательное число - расширяем знак
            raw |= ~((1 << size) - 1);
        }
        return static_cast<qint32>(raw);
    }
};
