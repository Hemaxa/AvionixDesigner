//ViewportSettingsPanel - панель настроек отображения сцены
 
#pragma once

#include "BasePanel.h"
#include <QColor>

class QLabel;
class QPushButton;

class ViewportSettingsPanel : public BasePanel
{
    Q_OBJECT
    
public:
    explicit ViewportSettingsPanel(QWidget *parent = nullptr);

signals:
    //сигнал об изменении цвета фона
    void bgColorChanged(const QColor &color);

public slots:
    void refreshInfo();

private slots:
    void onChangeBgColor();

private:
    QLabel *m_titleLabel;
    QLabel *m_sizeLabel;
    QLabel *m_bgColorPreview;
    QPushButton *m_bgColorButton;
};
