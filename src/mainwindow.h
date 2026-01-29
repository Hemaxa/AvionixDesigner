#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDockWidget>
#include <QList>
#include <QMap>
#include <QDomElement>
#include <QSharedPointer>
#include "shapes.h"
#include "CanvasWidget.h"
#include "ObjectListWidget.h"
#include "LogWidget.h"
#include "PropertiesWidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    
    bool loadXml(const QString &fileName);

private slots:
    void onObjectSelected(int index);
    void onOpenFile();

private:
    // Виджеты панелей
    CanvasWidget *m_canvas;
    ObjectListWidget *m_objectList;
    LogWidget *m_log;
    PropertiesWidget *m_properties;
    
    // Dock widgets
    QDockWidget *m_objectListDock;
    QDockWidget *m_logDock;
    QDockWidget *m_propertiesDock;
    
    // Данные
    QList<QSharedPointer<CorelShape>> m_shapes;
    QMap<QString, QMap<QString, ParamInfo>> m_schemas;
    
    // Параметры проекта
    int m_projectWidth;
    int m_projectHeight;
    QColor m_bgColor;

    void setupUI();
    void setupMenus();
    void parseParameters(const QDomElement &root);
    void parseObjects(const QDomElement &root);
    void log(const QString &msg);
};

#endif // MAINWINDOW_H