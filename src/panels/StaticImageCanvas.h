// StaticImageCanvas — увеличенный пиксельный предпросмотр, линейки, стыки и жест объединения.
#pragma once

#include "editor/StaticImageDocument.h"
#include <QSet>
#include <QTimer>
#include <QWidget>

// StaticImageCanvas — один вид документа; обзор и часть используют независимые масштаб/панораму.
class StaticImageCanvas : public QWidget
{
    Q_OBJECT
  public:
    explicit StaticImageCanvas(QWidget *parent = nullptr);
    void setDocument(const StaticImageDocument *document, int region = -1);
    void setEditable(bool editable);
    void setMergeTool(bool enabled)
    {
        m_mergeTool = enabled;
        update();
    }
    void setSelectedMasks(const QSet<int> &indexes);
    void fit();
    QTransform imageTransform() const;
    QRectF horizontalRulerRect() const;
    QRectF verticalRulerRect() const;
  signals:
    void splitRequested(int region, Qt::Orientation orientation, int coordinate);
    void exclusionRequested(int region);
    void regionsMergeRequested(const QList<int> &indexes);
    void masksMergeRequested(const QList<int> &indexes, int target);
    void masksSelected(const QList<int> &indexes, int target);
    void regionOpened(int region);
    void hintChanged(const QString &hint);

  protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void leaveEvent(QEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void keyReleaseEvent(QKeyEvent *) override;
    void focusOutEvent(QFocusEvent *) override;

  private:
    QRect contentRect() const;
    QPointF toImage(const QPointF &point) const;
    int maskAt(const QPointF &point) const;
    void updateHover(const QPointF &point);
    void collectStroke(const QPointF &point);
    void updateCut(const QPointF &point, bool lockRegion);
    void cancelGesture();
    const StaticImageDocument *m_document = nullptr;
    int m_region = -1, m_activeRegion = -1, m_hoverRegion = -1;
    QImage m_preview;
    QImage m_maskHighlight;
    bool m_editable = false, m_mergeTool = false, m_panning = false, m_stroking = false,
         m_cutDragging = false;
    QPointF m_pan, m_lastMouse, m_strokeLast;
    double m_zoom = 1.0;
    int m_cutRegion = -1, m_cutCoordinate = 0, m_strokeTarget = -1;
    int m_cutStartRegion = -1, m_pressedRegion = -1, m_cutKey = 0;
    QPointF m_pressPosition;
    Qt::Orientation m_cutOrientation = Qt::Vertical;
    bool m_cutPreview = false, m_extendPreview = false;
    QList<int> m_mergeBoundary;
    QLineF m_boundaryLine;
    QSet<int> m_stroke, m_selectedMasks;
    QTimer m_clickTimer;
    int m_clickRegion = -1;
    bool m_skipRelease = false;
};
