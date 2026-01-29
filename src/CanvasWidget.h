#pragma once

#include <QWidget>
#include <QList>
#include <QSharedPointer>
#include "shapes.h"

/**
 * @brief Виджет рабочей области для отрисовки объектов
 */
class CanvasWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CanvasWidget(QWidget *parent = nullptr);

    // Установить список фигур для отрисовки
    void setShapes(const QList<QSharedPointer<CorelShape>> &shapes);
    
    // Установить цвет фона
    void setBackgroundColor(const QColor &color);

    // Установить размер холста (из XML)
    void setCanvasSize(int width, int height);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QList<QSharedPointer<CorelShape>> m_shapes;
    QColor m_backgroundColor;
    int m_canvasWidth;
    int m_canvasHeight;
};