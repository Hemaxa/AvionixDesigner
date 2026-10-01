//FpgaScreenWidget - декоративная рамка экрана для результата симулятора ПЛИС

#pragma once

#include <QImage>
#include <QWidget>

class FpgaScreenWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FpgaScreenWidget(QWidget *parent = nullptr);

    void setFrameImage(const QImage &image);
    void setSimulationActive(bool active);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QRectF backingRect() const;

    QImage m_image;
    bool m_simulationActive = false;
};
