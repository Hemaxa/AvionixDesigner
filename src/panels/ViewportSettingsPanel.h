//ViewportSettingsPanel - компактная панель параметров рабочей области

#pragma once

#include "BasePanel.h"

#include <QColor>

class QLabel;
class QPushButton;
class QSpinBox;

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

private:
    QLabel *m_sizeLabel = nullptr;
    QPushButton *m_bgColorButton = nullptr;
    QSpinBox *m_widthSpin = nullptr;
    QSpinBox *m_heightSpin = nullptr;
    bool m_refreshing = false;
};
