//EditorProjectDocument - внутренняя модель проекта редактора

#pragma once

#include <QColor>
#include <QString>

class EditorProjectDocument
{
public:
    void clear();

    bool hasCanvas() const;

    QString projectName() const;
    void setProjectName(const QString &name);

    int canvasWidth() const;
    int canvasHeight() const;
    void setCanvasSize(int width, int height);

    QColor backgroundColor() const;
    void setBackgroundColor(const QColor &color);

private:
    QString m_projectName = QStringLiteral("Untitled");
    int m_canvasWidth = 0;
    int m_canvasHeight = 0;
    QColor m_backgroundColor = Qt::black;
};
