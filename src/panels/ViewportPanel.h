// ViewportPanel - панель рабочей области приложения.

#pragma once

#include "BasePanel.h"

#include <QList>
#include <QColor>
#include <QMap>

// ViewportPanel - панель рабочей области приложения.
class ViewportPanel : public BasePanel
{
    Q_OBJECT

public:
    explicit ViewportPanel(QWidget *parent = nullptr);

    // возвращает текущий масштаб
    double getScale() const;

    // возвращает индексы выделенных объектов
    QList<int> getSelectedIndexes() const { return m_selectedIndexes; }

    // сбрасывает вид к начальному состоянию
    void resetView();
    // Позиция вставки в координатах холста с учётом масштаба и панорамирования.
    QPointF pastePosition() const;

protected:
    // отрисовка панели
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

    // обработка мыши для выделения и изменения объектов
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

    // обработка колеса мыши для масштабирования
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
    void setProportionalResize(bool enabled);
    void setRotationSnapping(bool enabled);

private:
    QColor m_selectionColor = QColor("#00c8f0");
    double m_scale; //текущий масштаб
    double m_offsetX; //смещение по X
    double m_offsetY; //смещение по Y

    int m_selectedIndex = -1; // Индекс выделенного объекта
    QList<int> m_selectedIndexes; // Индексы выделенных объектов

    // DragMode - текущее действие мыши: выделение, перемещение, размер, поворот или панорамирование.
    enum DragMode { None, Move, Resize, Rotate, LineEndpoint, Pan, MarqueeSelect };
    DragMode m_dragMode = None;
    int m_resizeEdgeFlags = 0; // Битовая маска: 1=Left, 2=Right, 4=Top, 8=Bottom
    int m_lineEndpointIndex = -1;

    // SnapGuideKind - источник направляющей привязки: холст, сетка или объект.
    enum class SnapGuideKind { Canvas, Grid, Object };
    // SnapGuide - положение и направление одной линии привязки на холсте.
    struct SnapGuide
    {
        SnapGuideKind kind = SnapGuideKind::Object;
        Qt::Orientation orientation = Qt::Vertical;
        double position = 0.0;
    };

    // SnapResult - скорректированное перемещение и направляющие привязки.
    struct SnapResult
    {
        QPointF delta;
        QList<SnapGuide> guides;
    };

    // SnapPointResult - скорректированная точка и направляющие привязки.
    struct SnapPointResult
    {
        QPointF point;
        QList<SnapGuide> guides;
    };

    // SnapResizeResult - изменение размеров с учётом привязок и направляющих.
    struct SnapResizeResult
    {
        QPointF delta;
        QList<SnapGuide> guides;
    };

    QPointF m_lastMousePos; // Последняя позиция мыши для расчёта дельт
    QPointF m_dragStartCanvasPos;
    QRectF m_dragStartBounds;
    QPointF m_dragLastAppliedDelta;
    // Стартовые границы объектов для перемещения без накопления ошибок округления
    QMap<int, QRectF> m_dragStartObjectBounds;
    bool m_dragHistoryRecorded = false;
    bool m_dragEditedObjects = false;
    bool m_proportionalResize = false;
    bool m_rotationSnapping = false;
    QPointF m_rotationCenter;
    QList<SnapGuide> m_activeSnapGuides;
    QList<int> m_marqueeBaseSelection;
    QPointF m_marqueeStartPos;
    QPointF m_marqueeCurrentPos;

    // Преобразования
    QPointF mapToCanvas(const QPoint &widgetPoint) const;
    double currentTotalScale() const;
    double handleSizeInCanvas() const;

    // Отрисовка манипуляторов
    void drawManipulators(QPainter &painter, const QRectF &rect, bool canResize, bool canRotate);
    void drawDashedLineEndpointManipulators(QPainter &painter, const class DashedLineObject *line);
    void drawGrid(QPainter &painter, int canvasW, int canvasH, double totalScale);
    void drawSnapGuides(QPainter &painter, int canvasW, int canvasH);
    SnapResult snappedMoveDelta(int objectIndex, const QRectF &originalRect, const QPointF &delta) const;
    SnapResult snappedMoveDeltaForSelection(const QList<int> &objectIndexes, const QRectF &originalRect, const QPointF &delta) const;
    SnapPointResult snappedPoint(const QList<int> &excludedIndexes, const QPointF &point) const;
    SnapResizeResult snappedResizeDelta(int objectIndex, const QRectF &bounds, int edgeFlags, const QPointF &delta) const;
    int hitTestManipulators(const QPointF &canvasPos, const QRectF &rect, bool canResize, bool canRotate) const;
    int hitTestDashedLineEndpoints(const QPointF &canvasPos, const class DashedLineObject *line) const;
    QRectF selectedObjectsRect() const;
    bool selectedObjectContains(const QPointF &canvasPos) const;
    void beginMoveDrag(const QPointF &canvasPos, const QRectF &bounds);
    // Перемещает объект к позиции, рассчитанной от начала перетаскивания
    void moveObjectToDragDelta(int index, const QPointF &delta);
    void ensureDragHistoryRecorded();
    void storeViewState();
    void restoreViewState();
};
