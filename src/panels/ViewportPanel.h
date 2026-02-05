/**
 * @file ViewportPanel.h
 * @brief Панель холста для отображения объектов
 */

#pragma once

#include "../BasePanel.h"

/**
 * @class ViewportPanel
 * @brief Виджет холста для отрисовки объектов сцены
 */
class ViewportPanel : public BasePanel
{
    Q_OBJECT
    
public:
    explicit ViewportPanel(QWidget *parent = nullptr);
    
    // Устанавливает масштаб отображения
    void setScale(double scale);
    
    // Возвращает текущий масштаб
    double getScale() const;
    
    // Сбрасывает вид к начальному состоянию
    void resetView();

protected:
    // Отрисовка панели
    void paintEvent(QPaintEvent *event) override;
    
    // Обработка колеса мыши для масштабирования
    void wheelEvent(QWheelEvent *event) override;

private:
    double m_scale;    // Текущий масштаб
    double m_offsetX;  // Смещение по X
    double m_offsetY;  // Смещение по Y
};
