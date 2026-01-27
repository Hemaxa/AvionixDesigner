#ifndef UTILS_H
#define UTILS_H

#include <QString>
#include <QtMath>

class BitParser {
public:
    // Статический метод для извлечения значения из Hex-строки
    static quint32 extract(const QString &hexString, int offset, int size) {
        quint32 result = 0;
        int len = hexString.length();
        for (int i = 0; i < size; ++i) {
            int targetBitIndex = offset + i;
            int charIndex = len - 1 - (targetBitIndex / 4);
            if (charIndex < 0) break;

            QChar ch = hexString[charIndex];
            int val = 0;
            if (ch >= '0' && ch <= '9') val = ch.toLatin1() - '0';
            else if (ch >= 'A' && ch <= 'F') val = ch.toLatin1() - 'A' + 10;
            else if (ch >= 'a' && ch <= 'f') val = ch.toLatin1() - 'a' + 10;

            if ((val >> (targetBitIndex % 4)) & 1) {
                result |= (1 << i);
            }
        }
        return result;
    }
    
    // Помощник для конвертации BGR -> RGB
    static QColor parseColor(quint32 val) {
        int r = val & 0xFF;
        int g = (val >> 8) & 0xFF;
        int b = (val >> 16) & 0xFF;
        return QColor(r, g, b);
    }
};

// Структура для хранения схемы ("карты" битов)
struct ParamInfo {
    int offset;
    int size;
};

#endif // UTILS_H