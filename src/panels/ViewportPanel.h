//ViewportPanel - панель рабочей области прилосжения

#pragma once

#include "BasePanel.h"

#include <QList>

class ViewportPanel : public BasePanel
{
    Q_OBJECT
    
public:
    explicit ViewportPanel(QWidget *parent = nullptr);
    
    //устанавливает масштаб отображения
    void setScale(double scale);
    
    //возвращает текущий масштаб
    double getScale() const;
    
    //возвращает индекс выделенного объекта
    int getSelectedIndex() const { return m_selectedIndex; }
    QList<int> getSelectedIndexes() const { return m_selectedIndexes; }
    
    //сбрасывает вид к начальному состоянию
    void resetView();

protected:
    //отрисовка панели
    void paintEvent(QPaintEvent *event) override;
    
    //обработка мыши для выделения и изменения объектов
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    
    //обработка колеса мыши для масштабирования
    void wheelEvent(QWheelEvent *event) override;

signals:
    // Сигнал, испускаемый при выборе объекта кликом на холсте
    void objectSelected(int index);
    void selectionChanged(const QList<int> &indexes);
    // Сигнал, испускаемый при изменении свойств объекта перетаскиванием
    void objectChanged();
    void imageDropped(const QString &fileName);
    void objectDropped(const QString &typeName, const QPointF &canvasPos);

public slots:
    // Слот для внешней синхронизации выделения (например, из списка)
    void setSelectedIndex(int index);
    void setSelectedIndexes(const QList<int> &indexes);

private:
    double m_scale; //текущий масштаб
    double m_offsetX; //смещение по X
    double m_offsetY; //смещение по Y
    
    int m_selectedIndex = -1; // Индекс выделенного объекта
    QList<int> m_selectedIndexes; // Индексы выделенных объектов
    
    // Состояния манипуляторов
    enum DragMode { None, Move, Resize, Rotate, Pan, MarqueeSelect };
    DragMode m_dragMode = None;
    int m_resizeEdgeFlags = 0; // Битовая маска: 1=Left, 2=Right, 4=Top, 8=Bottom
    
    QPointF m_lastMousePos; // Последняя позиция мыши для расчёта дельт
    QPointF m_marqueeStartPos;
    QPointF m_marqueeCurrentPos;
    
    // Преобразования
    QPointF mapToCanvas(const QPoint &widgetPoint) const;
    
    // Отрисовка манипуляторов
    void drawManipulators(QPainter &painter, const QRectF &rect, bool canResize, bool canRotate);
    void drawGrid(QPainter &painter, int canvasW, int canvasH, double totalScale);
    QPointF snappedMoveDelta(int objectIndex, const QRectF &originalRect, const QPointF &delta) const;
    QPointF snappedMoveDeltaForSelection(const QList<int> &objectIndexes, const QRectF &originalRect, const QPointF &delta) const;
    int hitTestManipulators(const QPointF &canvasPos, const QRectF &rect, bool canResize, bool canRotate) const;
    QRectF selectedObjectsRect() const;
    bool selectedObjectContains(const QPointF &canvasPos) const;
};
