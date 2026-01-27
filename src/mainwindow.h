#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QList>
#include <QMap>
#include <QDomElement>
#include "shapes.h" // Подключаем наши классы фигур

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    bool loadXml(const QString &fileName);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    // Храним указатели на фигуры (полиморфизм)
    QList<QSharedPointer<CorelShape>> m_shapes; 
    
    // Схемы для разных типов объектов
    // Ключ 1: Тип объекта ("rectangle", "rotationobject")
    // Ключ 2: Имя поля ("x0", "sin")
    QMap<QString, QMap<QString, ParamInfo>> m_schemas;

    QString m_debugLog; 

    void parseParameters(const QDomElement &root);
    void parseObjects(const QDomElement &root);
    void log(const QString &msg);
};

#endif // MAINWINDOW_H