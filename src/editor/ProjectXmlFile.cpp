#include "ProjectXmlFile.h"

#include <QFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringDecoder>

namespace {
QString compactEncodingName(const QString &encoding)
{
    QString result = encoding.trimmed().toLower();
    result.remove(QLatin1Char('-'));
    result.remove(QLatin1Char('_'));
    result.remove(QLatin1Char(' '));
    return result;
}

bool isWindows1251Encoding(const QString &encoding)
{
    const QString compact = compactEncodingName(encoding);
    return compact == QStringLiteral("windows1251")
        || compact == QStringLiteral("cp1251")
        || compact == QStringLiteral("mscp1251")
        || compact == QStringLiteral("microsoftcp1251")
        || compact == QStringLiteral("xcp1251");
}

QString declaredXmlEncoding(const QByteArray &bytes)
{
    const QString header = QString::fromLatin1(bytes.left(1024));
    const QRegularExpression encodingRe(
        QStringLiteral("<\\?xml[^>]*\\bencoding\\s*=\\s*(['\\\"])([^'\\\"]+)\\1"),
        QRegularExpression::CaseInsensitiveOption
    );
    const QRegularExpressionMatch match = encodingRe.match(header);
    return match.hasMatch() ? match.captured(2).trimmed() : QString();
}

QString normalizeXmlDeclarationEncoding(QString text)
{
    const int declStart = text.indexOf(QStringLiteral("<?xml"), 0, Qt::CaseInsensitive);
    if (declStart < 0)
        return text;
    if (!text.left(declStart).trimmed().isEmpty())
        return text;

    const int declEnd = text.indexOf(QStringLiteral("?>"), declStart);
    if (declEnd < 0)
        return text;

    QString declaration = text.mid(declStart, declEnd + 2 - declStart);
    const QRegularExpression encodingRe(
        QStringLiteral("\\s+encoding\\s*=\\s*['\\\"][^'\\\"]+['\\\"]"),
        QRegularExpression::CaseInsensitiveOption
    );
    declaration.replace(encodingRe, QStringLiteral(" encoding=\"UTF-8\""));
    text.replace(declStart, declEnd + 2 - declStart, declaration);
    return text;
}

QString decodeWindows1251(const QByteArray &bytes)
{
    static constexpr ushort table80ToBF[] = {
        0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021,
        0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F,
        0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
        0xFFFD, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F,
        0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7,
        0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
        0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7,
        0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457
    };

    QString text;
    text.reserve(bytes.size());
    for (const char rawByte : bytes) {
        const uchar byte = static_cast<uchar>(rawByte);
        if (byte < 0x80) {
            text.append(QChar(byte));
        } else if (byte >= 0xC0) {
            text.append(QChar(static_cast<ushort>(byte < 0xE0
                ? 0x0410 + byte - 0xC0
                : 0x0430 + byte - 0xE0)));
        } else {
            text.append(QChar(table80ToBF[byte - 0x80]));
        }
    }
    return text;
}

QString decodeXmlWithQt(const QByteArray &bytes, QStringConverter::Encoding encoding, const QString &encodingName, QString *errorMessage)
{
    QStringDecoder decoder(encoding, QStringConverter::Flag::ConvertInitialBom);
    const QString text = decoder.decode(bytes);
    if (decoder.hasError()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Не удалось декодировать XML как %1").arg(encodingName);
        return QString();
    }
    return text;
}

QString decodeXmlWithNamedQtCodec(const QByteArray &bytes, const QString &encodingName, QString *errorMessage)
{
    const QByteArray codecName = encodingName.toLatin1();
    QStringDecoder decoder(codecName.constData(), QStringConverter::Flag::ConvertInitialBom);
    if (!decoder.isValid()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Кодировка XML \"%1\" не поддерживается этой сборкой Qt").arg(encodingName);
        return QString();
    }

    const QString text = decoder.decode(bytes);
    if (decoder.hasError()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Не удалось декодировать XML как %1").arg(encodingName);
        return QString();
    }
    return text;
}

QString decodeXmlBytes(const QByteArray &bytes, QString *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();

    const QString declaredEncoding = declaredXmlEncoding(bytes);
    if (isWindows1251Encoding(declaredEncoding))
        return decodeWindows1251(bytes);

    if (!declaredEncoding.isEmpty())
        return decodeXmlWithNamedQtCodec(bytes, declaredEncoding, errorMessage);

    const auto detectedEncoding = QStringConverter::encodingForData(bytes);
    if (detectedEncoding)
        return decodeXmlWithQt(bytes, *detectedEncoding, QString::fromLatin1(QStringConverter::nameForEncoding(*detectedEncoding)), errorMessage);

    return decodeXmlWithQt(bytes, QStringConverter::Utf8, QStringLiteral("UTF-8"), errorMessage);
}

int windows1251ByteForCodePoint(uint codePoint)
{
    if (codePoint <= 0x7F)
        return static_cast<int>(codePoint);
    if (codePoint >= 0x0410 && codePoint <= 0x042F)
        return 0xC0 + static_cast<int>(codePoint - 0x0410);
    if (codePoint >= 0x0430 && codePoint <= 0x044F)
        return 0xE0 + static_cast<int>(codePoint - 0x0430);

    switch (codePoint) {
    case 0x0402: return 0x80;
    case 0x0403: return 0x81;
    case 0x201A: return 0x82;
    case 0x0453: return 0x83;
    case 0x201E: return 0x84;
    case 0x2026: return 0x85;
    case 0x2020: return 0x86;
    case 0x2021: return 0x87;
    case 0x20AC: return 0x88;
    case 0x2030: return 0x89;
    case 0x0409: return 0x8A;
    case 0x2039: return 0x8B;
    case 0x040A: return 0x8C;
    case 0x040C: return 0x8D;
    case 0x040B: return 0x8E;
    case 0x040F: return 0x8F;
    case 0x0452: return 0x90;
    case 0x2018: return 0x91;
    case 0x2019: return 0x92;
    case 0x201C: return 0x93;
    case 0x201D: return 0x94;
    case 0x2022: return 0x95;
    case 0x2013: return 0x96;
    case 0x2014: return 0x97;
    case 0x2122: return 0x99;
    case 0x0459: return 0x9A;
    case 0x203A: return 0x9B;
    case 0x045A: return 0x9C;
    case 0x045C: return 0x9D;
    case 0x045B: return 0x9E;
    case 0x045F: return 0x9F;
    case 0x00A0: return 0xA0;
    case 0x040E: return 0xA1;
    case 0x045E: return 0xA2;
    case 0x0408: return 0xA3;
    case 0x00A4: return 0xA4;
    case 0x0490: return 0xA5;
    case 0x00A6: return 0xA6;
    case 0x00A7: return 0xA7;
    case 0x0401: return 0xA8;
    case 0x00A9: return 0xA9;
    case 0x0404: return 0xAA;
    case 0x00AB: return 0xAB;
    case 0x00AC: return 0xAC;
    case 0x00AD: return 0xAD;
    case 0x00AE: return 0xAE;
    case 0x0407: return 0xAF;
    case 0x00B0: return 0xB0;
    case 0x00B1: return 0xB1;
    case 0x0406: return 0xB2;
    case 0x0456: return 0xB3;
    case 0x0491: return 0xB4;
    case 0x00B5: return 0xB5;
    case 0x00B6: return 0xB6;
    case 0x00B7: return 0xB7;
    case 0x0451: return 0xB8;
    case 0x2116: return 0xB9;
    case 0x0454: return 0xBA;
    case 0x00BB: return 0xBB;
    case 0x0458: return 0xBC;
    case 0x0405: return 0xBD;
    case 0x0455: return 0xBE;
    case 0x0457: return 0xBF;
    default: return -1;
    }
}

QByteArray encodeXmlAsWindows1251(const QString &text)
{
    QByteArray encoded;
    encoded.reserve(text.size());

    for (int i = 0; i < text.size(); ++i) {
        uint codePoint = text.at(i).unicode();
        if (text.at(i).isHighSurrogate() && i + 1 < text.size() && text.at(i + 1).isLowSurrogate()) {
            codePoint = QChar::surrogateToUcs4(text.at(i), text.at(i + 1));
            ++i;
        }

        const int byte = windows1251ByteForCodePoint(codePoint);
        if (byte >= 0) {
            encoded.append(static_cast<char>(byte));
        } else {
            encoded.append("&#");
            encoded.append(QByteArray::number(codePoint));
            encoded.append(';');
        }
    }

    return encoded;
}

} // namespace

bool ProjectXmlFile::read(const QString &fileName, QDomDocument *doc, QString *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Не удалось открыть файл: %1").arg(file.errorString());
        return false;
    }

    const QByteArray bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Не удалось прочитать файл: %1").arg(file.errorString());
        return false;
    }

    QString decodeError;
    QString text = decodeXmlBytes(bytes, &decodeError);
    if (!decodeError.isEmpty()) {
        if (errorMessage)
            *errorMessage = decodeError;
        return false;
    }

    text = normalizeXmlDeclarationEncoding(text);
    const QDomDocument::ParseResult result = doc->setContent(text, QDomDocument::ParseOption::PreserveSpacingOnlyNodes);
    if (!result) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Ошибка XML: %1 (строка %2, столбец %3)")
                .arg(result.errorMessage)
                .arg(result.errorLine)
                .arg(result.errorColumn);
        }
        return false;
    }

    return true;
}

bool ProjectXmlFile::write(const QString &fileName, const QDomDocument &document, QString *errorMessage)
{
    QString ignored;
    QString &error = errorMessage ? *errorMessage : ignored;
    error.clear();
    QSaveFile outFile(fileName);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        error = QStringLiteral("Не удалось открыть файл для записи: %1").arg(outFile.errorString());
        return false;
    }

    const QString xmlText = QStringLiteral("<?xml version='1.0' encoding='windows-1251'?>\n") + document.toString(4);
    const QByteArray encodedXml = encodeXmlAsWindows1251(xmlText);
    if (outFile.write(encodedXml) != encodedXml.size()) {
        error = QStringLiteral("Не удалось записать XML: %1").arg(outFile.errorString());
        outFile.cancelWriting();
        return false;
    }
    if (!outFile.commit()) {
        error = QStringLiteral("Не удалось завершить сохранение XML: %1").arg(outFile.errorString());
        return false;
    }

    return true;
}
