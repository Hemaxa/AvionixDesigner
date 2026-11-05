#pragma once

#include "BasePanel.h"
#include <QSet>

class QScrollArea;
class QVBoxLayout;
class QToolButton;
class QTimer;

// Только эта панель управляет слоями; остальные поля позволяют лишь выбрать слой.
class LayersPanel : public BasePanel
{
    Q_OBJECT
public:
    explicit LayersPanel(QWidget *parent = nullptr);
    void refreshLayers();
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    void refreshViewState();
    void selectLayer(int id, Qt::KeyboardModifiers modifiers);
    void mergeSelectedLayers();
    void deleteLayer(int id);
    QSet<int> m_selectedLayerIds;
    int m_selectionAnchor = -1;
    int m_knownActiveLayer = -1;
    bool m_selectingLayer = false;
    bool m_rebuilding = false;
    QScrollArea *m_scroll = nullptr;
    QWidget *m_rows = nullptr;
    QVBoxLayout *m_rowsLayout = nullptr;
    QTimer *m_refreshTimer = nullptr;
    QToolButton *m_allLayers = nullptr;
};
