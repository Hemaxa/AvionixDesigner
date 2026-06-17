//TextObject - редакторский объект текста, сохраняемый как staticgroup

#pragma once

#include "StaticGroupObject.h"

class TextObject : public StaticGroupObject
{
    Q_OBJECT

public:
    QString text = QStringLiteral("TEXT");
    QString fontFamily = QStringLiteral("Arial");
    int pixelSize = 28;
    bool bold = false;
    bool italic = false;
    bool underline = false;

    explicit TextObject(QObject *parent = nullptr);

    QString getTypeName() const override;
    QString getDisplayName() const override;
    QList<QPair<QString, QString>> getProperties() const override;
    bool setObjectProperty(const QString &name, const QString &value) override;
    void resizeBy(int edgeFlags, double dx, double dy) override;

private:
    void rebuildMask();
};
