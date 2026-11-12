#pragma once

#include <QDomDocument>
#include <QString>

// ProjectXmlFile — чтение XML с объявленной кодировкой и атомарная запись
// в Windows-1251. Символы вне кодовой страницы сохраняются XML-ссылками.
class ProjectXmlFile
{
public:
    static bool read(const QString &fileName, QDomDocument *document, QString *errorMessage);
    static bool write(const QString &fileName, const QDomDocument &document, QString *errorMessage);
};
