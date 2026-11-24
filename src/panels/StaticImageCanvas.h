// StaticImageCanvas — увеличенный пиксельный предпросмотр, линейки, стыки и жест объединения.
#pragma once

#include "editor/StaticImageDocument.h"
#include <QHash>
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
    void setBeforeImage(const QImage &image);
    void setShowBefore(bool before);
    void setIsolated(bool isolated);
    void setHandTool(bool enabled);
    void setExtendTool(bool enabled);
    void setEraserTool(bool enabled);
    void setBrushSize(int size);
    void focusNextFragment();
    bool hasFragments() const
    {
        return !m_fragments.isEmpty();
    }
    void setExactCut(int region, Qt::Orientation orientation, int coordinate);
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
    void selectionInfoChanged(const QString &info);
    void cutsExtensionRequested(const QList<int> &regions, Qt::Orientation orientation, int coordinate);
    void eraseFinished(const StaticImageDocument &document);

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
    void findExtension(const QPointF &point);
    void eraseTo(const QPointF &point);
    const StaticImageDocument *m_document = nullptr;
    int m_region = -1, m_activeRegion = -1, m_hoverRegion = -1;
    QImage m_preview;
    QImage m_maskHighlight;
    QImage m_before, m_isolatedImage;
    QHash<int, StaticMaskAnalysis> m_analysis;
    QList<QRect> m_fragments;
    int m_fragment = -1;
    StaticImageDocument m_eraseDraft;
    bool m_showBefore = false, m_isolated = false, m_handTool = false, m_extendTool = false,
         m_eraserTool = false, m_erasing = false, m_eraseChanged = false, m_spaceHeld = false;
    int m_brushSize = 1;
    QList<int> m_extensionRegions;
    bool m_exactCut = false;
    int m_exactRegion = -1, m_exactCoordinate = 0;
    Qt::Orientation m_exactOrientation = Qt::Vertical;
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
