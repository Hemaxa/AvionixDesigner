// ElidedLabel — однострочная подпись с многоточием вместо обрезанных букв.
#pragma once

#include <QLabel>
#include <QPainter>

class ElidedLabel : public QLabel
{
public:
    ElidedLabel(const QString &text, QWidget *parent) : QLabel(text, parent) {
        setTextFormat(Qt::PlainText);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        setMinimumWidth(0);
        setToolTip(text);
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setPen(palette().color(foregroundRole()));
        const QRect area = contentsRect();
        painter.drawText(area, alignment(), fontMetrics().elidedText(text(), Qt::ElideRight, area.width()));
    }
};
