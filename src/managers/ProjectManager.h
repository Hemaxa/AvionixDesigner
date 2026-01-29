//ProjectManager - менеджер проекта, который хранит все данные загруженного XML

#pragma once //защита от повторного включения файла

#include <QObject>
#include <QList>
#include <QSharedPointer> //умный указатель, который автоматически удаляет объекты
#include <QColor>

#include "../objects/AbstractObject.h"

class ProjectManager : public QObject
{
    Q_OBJECT
    
public:
    //статический метод для доступа к единственному экземпляру (singleton)
    static ProjectManager* instance();
    
    //метод загрузки проекта из файла
    bool loadFromFile(const QString &fileName);
    
    //метод регистрации стандартных типов объектов
    void registerStandardTypes();

    //геттеры объектов
    const QList<QSharedPointer<AbstractObject>>& getObjects() const; //список объектов
    QSharedPointer<AbstractObject> getObjectAt(int index) const; //метод получения конкретного объекта по его индексу
    int getObjectCount() const; //метод получения количества объектов

    //геттеры свойств проекта
    QString getProjectName() const;
    int getCanvasWidth() const;
    int getCanvasHeight() const;
    QColor getBackgroundColor() const;
    QString getFilePath() const;

signals:
    //сигналы об изменении проекта
    void projectLoaded();
    void projectChanged();
    void logMessage(const QString &message);

private:
    //приватный конструктор
    ProjectManager();
    
    //поля свойств проекта
    QString m_projectName;
    int m_canvasWidth;
    int m_canvasHeight;
    QColor m_bgColor;
    QString m_filePath;

    //список объектов
    QList<QSharedPointer<AbstractObject>> m_objects;
};
