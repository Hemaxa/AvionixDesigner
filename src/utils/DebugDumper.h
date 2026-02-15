//DebugDumper - служебный класс для формирования отладочного дампа парсинга объектов

#pragma once

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>
#include <QList>
#include <QSharedPointer>
#include <QDomElement>

#include "BaseObject.h"
#include "BitParser.h"

class DebugDumper
{
public:
    /**
     * @brief Формирует текстовый файл с результатами парсинга всех объектов
     * @param filePath - путь к исходному XML-файлу проекта
     * @param objects - список распарсенных объектов
     * @param schemas - карта битовых схем для каждого типа
     * @param objectElements - список DOM-элементов объектов (для извлечения HEX-строк)
     * @param objectTypes - список имён типов объектов (tagName из XML)
     */
    static void dumpToFile(const QString &filePath,
                           const QList<QSharedPointer<BaseObject>> &objects,
                           const QMap<QString, ParamSchema> &schemas,
                           const QList<QDomElement> &objectElements,
                           const QStringList &objectTypes)
    {
        //определяем путь к выходному файлу: tests/<имя_файла>_debug.txt
        QFileInfo fi(filePath);
        QString testsDir = fi.absolutePath();
        
        //если файл не в tests/, ищем папку tests рядом с проектом
        if (!testsDir.endsWith("/tests")) {
            //пробуем найти tests/ относительно корня проекта
            QDir projectDir(fi.absolutePath());
            if (projectDir.exists("tests")) {
                testsDir = projectDir.absoluteFilePath("tests");
            } else if (projectDir.cdUp() && projectDir.exists("tests")) {
                testsDir = projectDir.absoluteFilePath("tests");
            } else {
                //создаём папку tests рядом с файлом проекта
                testsDir = fi.absolutePath() + "/tests";
                QDir().mkpath(testsDir);
            }
        }

        QString outputPath = testsDir + "/" + fi.baseName() + "_debug.txt";

        QFile outFile(outputPath);
        if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
            return;

        QTextStream out(&outFile);

        //заголовок файла
        out << QString("=").repeated(80) << "\n";
        out << "ОТЛАДОЧНЫЙ ДАМП ПАРСИНГА: " << fi.fileName() << "\n";
        out << "Всего объектов: " << objects.size() << "\n";
        out << QString("=").repeated(80) << "\n\n";

        //проходим по каждому объекту
        for (int i = 0; i < objects.size(); ++i) {
            auto obj = objects[i];
            QString typeName = (i < objectTypes.size()) ? objectTypes[i] : "unknown";
            QString hexInit;

            //извлекаем HEX-строку из DOM-элемента
            if (i < objectElements.size()) {
                hexInit = objectElements[i].firstChildElement("init").text().trimmed();
            }

            out << QString("-").repeated(80) << "\n";
            out << "Объект #" << (i + 1) 
                << "  |  тег: " << typeName 
                << "  |  класс: " << obj->getTypeName() << "\n";
            out << QString("-").repeated(80) << "\n\n";

            //исходная HEX-строка
            out << "HEX:  " << hexInit << "\n";

            //перевод в двоичную систему
            QString binString = hexToBinary(hexInit);
            out << "BIN:  " << binString << "\n";
            out << "Длина: " << binString.length() << " бит\n\n";

            //побитовая раскладка параметров
            if (schemas.contains(typeName)) {
                const ParamSchema &schema = schemas[typeName];

                out << "РАСКЛАДКА ПО БИТАМ:\n\n";

                //сортируем параметры по offset
                QList<QString> paramNames = schema.keys();
                std::sort(paramNames.begin(), paramNames.end(), 
                    [&schema](const QString &a, const QString &b) {
                        return schema[a].offset < schema[b].offset;
                    });

                for (const QString &paramName : paramNames) {
                    const ParamInfo &info = schema[paramName];
                    quint32 rawValue = 0;
                    
                    if (!hexInit.isEmpty()) {
                        rawValue = BitParser::extract(hexInit, info.offset, info.size);
                    }

                    //выделяем соответствующие биты
                    QString bitSlice = extractBitSlice(binString, info.offset, info.size);

                    out << "  [" << bitSlice << "]"
                        << "  ->  " << paramName
                        << "  =  " << rawValue
                        << "  (0x" << QString::number(rawValue, 16).toUpper() << ")"
                        << "    |  бит " << info.offset << ".." << (info.offset + info.size - 1) 
                        << " (" << info.size << " бит)\n";
                }
                out << "\n";
            }

            //итоговые свойства объекта
            out << "СВОЙСТВА ПОСЛЕ ПАРСИНГА:\n\n";

            const auto props = obj->getProperties();
            for (const auto &prop : props) {
                out << "  " << prop.first << " = " << prop.second << "\n";
            }

            out << "\n";
        }

        out << QString("=").repeated(80) << "\n";
        out << "  КОНЕЦ ДАМПА\n";
        out << QString("=").repeated(80) << "\n";

        outFile.close();
    }

private:
    //преобразование HEX-строки в двоичную строку
    static QString hexToBinary(const QString &hex)
    {
        QString result;
        for (int i = 0; i < hex.length(); ++i) {
            QChar ch = hex[i];
            int val = 0;
            if (ch >= '0' && ch <= '9')
                val = ch.toLatin1() - '0';
            else if (ch >= 'A' && ch <= 'F')
                val = ch.toLatin1() - 'A' + 10;
            else if (ch >= 'a' && ch <= 'f')
                val = ch.toLatin1() - 'a' + 10;

            //каждый HEX-символ = 4 бита
            for (int b = 3; b >= 0; --b) {
                result += ((val >> b) & 1) ? '1' : '0';
            }
        }
        return result;
    }

    //извлечение среза битов из BIN-строки (с учётом порядка BitParser: с конца)
    static QString extractBitSlice(const QString &binString, int offset, int size)
    {
        //BitParser работает с конца строки, поэтому инвертируем индексацию
        int totalBits = binString.length();
        QString slice;

        for (int i = 0; i < size; ++i) {
            int targetBit = offset + i;
            //BitParser считает биты с конца HEX-строки
            int binIndex = totalBits - 1 - targetBit;
            if (binIndex >= 0 && binIndex < totalBits) {
                slice.prepend(binString[binIndex]);
            } else {
                slice.prepend('?');
            }
        }
        return slice;
    }
};
