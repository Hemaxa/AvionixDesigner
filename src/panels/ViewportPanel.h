//ViewportPanel - панель рабочей области прилосжения

#pragma once

#include "BasePanel.h"

class ViewportPanel : public BasePanel
{
    Q_OBJECT
    
public:
    explicit ViewportPanel(QWidget *parent = nullptr);
    
    //устанавливает масштаб отображения
    void setScale(double scale);
    
    //возвращает текущий масштаб
    double getScale() const;
    
    //сбрасывает вид к начальному состоянию
    void resetView();

protected:
    //отрисовка панели
    void paintEvent(QPaintEvent *event) override;
    
    //обработка колеса мыши для масштабирования
    void wheelEvent(QWheelEvent *event) override;

private:
    double m_scale; //текущий масштаб
    double m_offsetX; //смещение по X
    double m_offsetY; //смещение по Y
};
