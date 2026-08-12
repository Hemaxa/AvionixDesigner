//ViewportSettingsPanel - компактная панель параметров рабочей области

#pragma once

#include "BasePanel.h"

#include <QColor>

class QBoxLayout;
class QFrame;
class QLabel;
class QPushButton;
class QResizeEvent;
class QSpinBox;
class QToolButton;

class ViewportSettingsPanel : public BasePanel
{
    Q_OBJECT

public:
    explicit ViewportSettingsPanel(QWidget *parent = nullptr);

signals:
    void bgColorChanged(const QColor &color);
    void canvasSizeChanged(int width, int height);

public slots:
    void refreshInfo();

private slots:
    void onChangeBgColor();
    void onCanvasSizeEdited();
    void onToggleGrid(bool enabled);
    void onToggleSnapCanvas(bool enabled);
    void onToggleSnapGrid(bool enabled);
    void onToggleSnapObjects(bool enabled);

private:
    void resizeEvent(QResizeEvent *event) override;
    void updateAdaptiveLayout();

    QBoxLayout *m_layout = nullptr;
    QBoxLayout *m_canvasLayout = nullptr;
    QBoxLayout *m_snapLayout = nullptr;
    QFrame *m_separator = nullptr;
    QPushButton *m_bgColorButton = nullptr;
    QSpinBox *m_widthSpin = nullptr;
    QSpinBox *m_heightSpin = nullptr;
    QToolButton *m_gridButton = nullptr;
    QToolButton *m_snapCanvasButton = nullptr;
    QToolButton *m_snapGridButton = nullptr;
    QToolButton *m_snapObjectsButton = nullptr;
    bool m_verticalLayout = false;
    bool m_refreshing = false;
};
