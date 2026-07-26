//EditorProjectDocument - внутренняя модель проекта редактора

#pragma once

#include <QColor>
#include <QString>

class EditorProjectDocument
{
public:
    //сбрасывает документ к пустому состоянию
    void clear();

    //проверяет, задана ли рабочая область проекта
    bool hasCanvas() const;

    //возвращает имя проекта
    QString projectName() const;
    //задаёт имя проекта
    void setProjectName(const QString &name);

    //возвращает ширину рабочей области
    int canvasWidth() const;
    //возвращает высоту рабочей области
    int canvasHeight() const;
    //задаёт размер рабочей области
    void setCanvasSize(int width, int height);

    //возвращает цвет фона рабочей области
    QColor backgroundColor() const;
    //задаёт цвет фона рабочей области
    void setBackgroundColor(const QColor &color);

private:
    QString m_projectName = QStringLiteral("Untitled");
    int m_canvasWidth = 0;
    int m_canvasHeight = 0;
    QColor m_backgroundColor = Qt::black;
};
