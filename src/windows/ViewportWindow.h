/**
 * @file ViewportWindow.h
 * @brief Окно с холстом для отображения объектов
 */

#pragma once

#include <QWidget>

/**
 * @class ViewportWindow
 * @brief Виджет холста для отрисовки объектов
 */
class ViewportWindow : public QWidget
{
    Q_OBJECT
    
public:
    explicit ViewportWindow(QWidget *parent = nullptr);
    
    void setScale(double scale);
    double getScale() const;
    void resetView();

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    double m_scale;
    double m_offsetX;
    double m_offsetY;
};
