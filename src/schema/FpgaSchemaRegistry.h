//FpgaSchemaRegistry - единая точка описания XML/FPGA-схем и UI-каталога объектов

#pragma once

#include <QList>
#include <QMap>
#include <QString>

#include "BaseObject.h"

struct FpgaSchemaField
{
    QString name;
    int offset = 0;
    int size = 0;
};

struct EditorObjectDescriptor
{
    QString typeName;
    QString title;
    QString iconPath;
    bool creatableInLibrary = false;
    bool creatableInMenu = false;
};

class FpgaSchemaRegistry
{
public:
    static FpgaSchemaRegistry* instance();

    QString canonicalSchemaName(const QString &typeName) const;
    QString canonicalObjectTag(const QString &typeName) const;

    QList<FpgaSchemaField> fieldsForSchema(const QString &schemaName) const;
    ParamSchema buildSchema(const QString &schemaName) const;

    QStringList orderedSchemaNames() const;
    QStringList defaultSchemaNames() const;
    QMap<QString, QString> defaultSchemaAliases() const;

    QList<EditorObjectDescriptor> editorObjectCatalog() const;

private:
    FpgaSchemaRegistry() = default;
};
