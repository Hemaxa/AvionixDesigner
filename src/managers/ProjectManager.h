//ProjectManager - менеджер проекта, который хранит все данные загруженного XML

#pragma once

#include <QObject>
#include <QList>
#include <QSharedPointer>
#include <QColor>

#include "BaseObject.h"

class ProjectManager : public QObject
{
    Q_OBJECT
    
public:
    //получение единственного экземпляра
    static ProjectManager* instance();
    
    //метод загрузки проекта из файла
    bool loadFromFile(const QString &fileName);
    
    //метод регистрации стандартных типов объектов
    void registerStandardTypes();

    //геттеры объектов
    const QList<QSharedPointer<BaseObject>>& getObjects() const;
    QSharedPointer<BaseObject> getObjectAt(int index) const;
    int getObjectCount() const;

    //геттеры свойств проекта
    QString getProjectName() const;
    int getCanvasWidth() const;
    int getCanvasHeight() const;
    QColor getBackgroundColor() const;
    QString getFilePath() const;

signals:
    void projectLoaded(); //проект загружен
    void projectChanged(); //проект изменён
    void logMessage(const QString &message); //сообщение для лога

private:
    //конструктор
    ProjectManager();
    
    QString m_projectName; //имя проекта
    int m_canvasWidth; //ширина холста
    int m_canvasHeight; //высота холста
    QColor m_bgColor; //цвет фона
    QString m_filePath; //путь к файлу

    QList<QSharedPointer<BaseObject>> m_objects; //список объектов (указатели на все объекты)
};
