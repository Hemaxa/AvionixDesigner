//BitParser - служебный класс с утилитами для парсинга битовых данных из HEX-строк

#pragma once

#include <QString>
#include <QColor> //для преобразованного BRG в RGB

class BitParser 
{
public:
    //метод, который извлекает числовое значение из HEX-строки
    static quint32 extract(const QString &hexString, int offset, int size) 
    {
        //полученное извлеченное число
        quint32 result = 0;

        //длина поступившей на вход HEX-строки
        int len = hexString.length();
        
        //цикл по идексам внутри диапазона нужных символов из HEX-строки
        for (int i = 0; i < size; ++i) {
            //глобальный индекс бита в двоичном виде строки (идет с конца)
            int targetBitIndex = offset + i;
            
            //индекс символа в строке в исходном, шестнадцатеричном виде (идет с начала)
            int charIndex = len - 1 - (targetBitIndex / 4);
            
            //защита от выхода за границы
            if (charIndex < 0) break;

            //преобразуем HEX-символ в число (0-15)
            QChar ch = hexString[charIndex];
            int val = 0;
            if (ch >= '0' && ch <= '9') 
                //ноль вычитается, так как мы смещаемся по таблице ASCII из строковой зоны в числовую (код символа превращается в значение цифры)
                val = ch.toLatin1() - '0';
            else if (ch >= 'A' && ch <= 'F') 
                val = ch.toLatin1() - 'A' + 10;
            else if (ch >= 'a' && ch <= 'f') 
                val = ch.toLatin1() - 'a' + 10;

            //определяем, какой бит внутри символа нам нужен (0..3), т.к. на один charIndex приходится 4 targetBitIndex
            int bitInChar = targetBitIndex % 4;
            
            //если бит установлен, добавляем его в результат
            if ((val >> bitInChar) & 1) {
                result |= (1 << i);
            }
        }
        return result;
    }
    
    //метод, который конвертирует BGR значение в QColor
    static QColor parseColor(quint32 bgrValue) 
    {
        int r = bgrValue & 0xFF; //берем красный из конца (0000 0000 0000 0000 1111 1111)
        int g = (bgrValue >> 8) & 0xFF; //зеленый в середине, сдвигаемся на 8 бит с конца (0000 0000 1111 1111 0000 0000)
        int b = (bgrValue >> 16) & 0xFF; //синий в начале, сдвигаемся на 16 бит с конца (1111 1111 0000 0000 0000 0000)
        return QColor(r, g, b);
    }
    
    //метод, который извлекает знаковое числовое значение из HEX-строки
    //используется для значений sin/cos в RotationObject, где требуется знаковая интерпретация битов
    static qint32 extractSigned(const QString &hexString, int offset, int size)
    {
        quint32 raw = extract(hexString, offset, size);
        
        //проверяем старший бит для определения знака
        if (raw & (1 << (size - 1))) {
            //отрицательное число - расширяем знак
            raw |= ~((1 << size) - 1);
        }
        return static_cast<qint32>(raw);
    }
};
