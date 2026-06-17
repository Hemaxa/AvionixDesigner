#include "FpgaSchemaRegistry.h"

namespace {
QList<FpgaSchemaField> rectangleFields()
{
    return {
        {"enb", 0, 1},
        {"color", 1, 24},
        {"colorb", 25, 24},
        {"x0", 49, 12},
        {"y0", 61, 12},
        {"w", 73, 12},
        {"h", 85, 12},
        {"a", 97, 12},
        {"padding", 109, 41}
    };
}

QList<FpgaSchemaField> rotationObjectFields()
{
    return {
        {"enb", 0, 1},
        {"color", 1, 24},
        {"xrot", 25, 12},
        {"yrot", 37, 12},
        {"top", 49, 12},
        {"left", 61, 12},
        {"bottom", 73, 12},
        {"right", 85, 12},
        {"sq", 97, 8},
        {"sin", 105, 18},
        {"cos", 123, 18},
        {"padding", 141, 9}
    };
}

QList<FpgaSchemaField> staticGroupFields()
{
    return {
        {"enb", 0, 1},
        {"color", 1, 24},
        {"x", 25, 12},
        {"y", 37, 12},
        {"w", 49, 12},
        {"h", 61, 12},
        {"addr", 73, 16},
        {"padding", 89, 61}
    };
}

QList<FpgaSchemaField> aviaHorizonFields()
{
    return {
        {"enb", 0, 1},
        {"earth", 1, 24},
        {"sky", 25, 24},
        {"hline", 49, 24},
        {"width", 73, 4},
        {"xo", 77, 12},
        {"yo", 89, 12},
        {"sn", 101, 18},
        {"cs", 119, 18},
        {"padding", 137, 13}
    };
}
}

FpgaSchemaRegistry* FpgaSchemaRegistry::instance()
{
    static FpgaSchemaRegistry registry;
    return &registry;
}

QString FpgaSchemaRegistry::canonicalSchemaName(const QString &typeName) const
{
    if (typeName == "rectangle_a")
        return QStringLiteral("rectangle");
    if (typeName == "aviahorizont")
        return QStringLiteral("aviagorizont");
    if (typeName == "text")
        return QStringLiteral("staticgroup");
    return typeName;
}

QString FpgaSchemaRegistry::canonicalObjectTag(const QString &typeName) const
{
    if (typeName == "rectangle_a")
        return QStringLiteral("rectangle");
    if (typeName == "aviahorizont")
        return QStringLiteral("aviagorizont");
    if (typeName == "text")
        return QStringLiteral("staticgroup");
    return typeName;
}

QList<FpgaSchemaField> FpgaSchemaRegistry::fieldsForSchema(const QString &schemaName) const
{
    if (schemaName == "rectangle")
        return rectangleFields();
    if (schemaName == "rotationobject")
        return rotationObjectFields();
    if (schemaName == "staticgroup")
        return staticGroupFields();
    if (schemaName == "aviagorizont")
        return aviaHorizonFields();
    return {};
}

ParamSchema FpgaSchemaRegistry::buildSchema(const QString &schemaName) const
{
    ParamSchema schema;
    const auto fields = fieldsForSchema(schemaName);
    for (const auto &field : fields) {
        schema.insert(field.name, {field.offset, field.size});
    }
    return schema;
}

QStringList FpgaSchemaRegistry::orderedSchemaNames() const
{
    return {"rectangle", "rotationobject", "staticgroup", "aviagorizont"};
}

QStringList FpgaSchemaRegistry::defaultSchemaNames() const
{
    return orderedSchemaNames();
}

QMap<QString, QString> FpgaSchemaRegistry::defaultSchemaAliases() const
{
    return {
        {"rectangle_a", "rectangle"},
        {"aviahorizont", "aviagorizont"}
    };
}

QList<EditorObjectDescriptor> FpgaSchemaRegistry::editorObjectCatalog() const
{
    return {
        {"rectangle", QStringLiteral("Прямоугольник"), QStringLiteral(":/icons/icons/library/rectangle.svg"), true, true},
        {"aviagorizont", QStringLiteral("Авиагоризонт"), QStringLiteral(":/icons/icons/library/aviahorizon.svg"), true, true},
        {"text", QStringLiteral("Текст"), QStringLiteral(":/icons/icons/library/text.svg"), true, true},
        {"staticgroup", QStringLiteral("Static"), QStringLiteral(":/icons/icons/library/staticgroup.svg"), false, false},
        {"rotationobject", QStringLiteral("Rotation Group"), QStringLiteral(":/icons/icons/library/rotationgroup.svg"), false, false}
    };
}
