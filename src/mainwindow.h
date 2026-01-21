//единоразовое включение заголовочного файла при компиляции
#pragma once

#include <QMainWindow>
#include <QList>
#include <QMap>
#include <QDomElement> //работа с XML в Qt

//расшифрованный объект
struct Rectangle {
    double x0, y0, w, h; //координаты и размеры
    QColor color; //цвет заливки
    QColor colorb; //цвет рамки
    double a; //толщина рамки
};

//один параметр
struct Parameter {
    int offset;
    int size;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    //метод загрузки файла
    bool loadFile(const QString &fileName);

protected:
    //метод отрисовки
    void paintEvent(QPaintEvent *event) override;

private:
    //список объектов
    QList<Rectangle> m_rects;

    //схема параметров
    QMap<QString, Parameter> m_rectSchema;
    
    //переменная для лога
    QString m_debugLog; 

    void parseParameters(const QDomElement &root);
    void parseObjects(const QDomElement &root);
    quint32 extractValue(const QString &hexString, int offset, int size);
    
    //помощник для логирования
    void log(const QString &msg);
};