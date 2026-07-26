//FpgaSchemaRegistry - единая точка описания XML/FPGA-схем и UI-каталога объектов

#pragma once

#include <QList>
#include <QMap>
#include <QString>

#include "BaseObject.h"

struct FpgaSchemaField
{
    QString name; //имя параметра в аппаратной схеме
    int offset = 0; //смещение параметра в битах
    int size = 0; //размер параметра в битах
};

//описание объекта, доступного пользователю в библиотеке/меню редактора
struct EditorObjectDescriptor
{
    QString typeName; //внутреннее имя типа объекта
    QString title; //название для интерфейса
    QString iconPath; //путь к иконке в ресурсах Qt
    bool creatableInLibrary = false; //показывать ли объект в панели библиотеки
    bool creatableInMenu = false; //показывать ли объект в меню создания
};

class FpgaSchemaRegistry
{
public:
    //возвращает единственный экземпляр реестра схем
    static FpgaSchemaRegistry* instance();

    //возвращает каноническое имя схемы параметров для типа объекта
    QString canonicalSchemaName(const QString &typeName) const;
    //возвращает каноническое имя XML-тега объекта для типа объекта
    QString canonicalObjectTag(const QString &typeName) const;

    //возвращает поля схемы в аппаратном порядке
    QList<FpgaSchemaField> fieldsForSchema(const QString &schemaName) const;
    //строит ParamSchema для парсинга и сериализации HEX-параметров
    ParamSchema buildSchema(const QString &schemaName) const;

    //возвращает полный порядок известных схем
    QStringList orderedSchemaNames() const;
    //возвращает схемы, которые должны присутствовать в новом проекте
    QStringList defaultSchemaNames() const;
    //возвращает стандартные псевдонимы имён схем
    QMap<QString, QString> defaultSchemaAliases() const;

    //возвращает каталог объектов, доступных в интерфейсе редактора
    QList<EditorObjectDescriptor> editorObjectCatalog() const;

private:
    FpgaSchemaRegistry() = default;
};
