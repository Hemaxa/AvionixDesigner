//NewProjectDialog - диалог создания нового проекта

#pragma once

#include <QColor>
#include <QDialog>

class QLineEdit;
class QPushButton;
class QSpinBox;

class NewProjectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewProjectDialog(QWidget *parent = nullptr);

    QString projectName() const;
    int canvasWidth() const;
    int canvasHeight() const;
    QColor backgroundColor() const;

private slots:
    void chooseBackgroundColor();

private:
    void refreshColorPreview();

    QLineEdit *m_nameEdit = nullptr;
    QSpinBox *m_widthSpin = nullptr;
    QSpinBox *m_heightSpin = nullptr;
    QPushButton *m_colorButton = nullptr;

    QColor m_backgroundColor = Qt::black;
};
