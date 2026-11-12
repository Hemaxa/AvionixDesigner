#pragma once

#include "BaseObject.h"
#include "EditorProjectDocument.h"
#include <QDomDocument>
#include <QSharedPointer>

// FpgaXmlExporter — формирование аппаратных parameters/objects из сцены
// редактора: init/data, цветовые маски и ресурсы font/text. Метаданные
// <editor> и запись файла относятся к отдельным компонентам.
class FpgaXmlExporter
{
public:
    static QDomDocument compile(const EditorProjectDocument &document,
                               const QList<QSharedPointer<BaseObject>> &objects,
                               const QStringList &objectTags);
};
