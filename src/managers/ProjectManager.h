/**
 * @file ProjectManager.h
 * @brief Менеджер проекта - хранит все данные загруженного XML
 */

#pragma once

#include <QObject>
#include <QList>
#include <QSharedPointer>
#include <QColor>

#include "../BaseObject.h"

/**
 * @class ProjectManager
 * @brief Менеджер проекта (синглтон) - загрузка и хранение данных проекта
 */
class ProjectManager : public QObject
{
    Q_OBJECT
    
public:
    // Получение единственного экземпляра
    static ProjectManager* instance();
    
    // Загружает проект из файла
    bool loadFromFile(const QString &fileName);
    
    // Регистрирует стандартные типы объектов
    void registerStandardTypes();

    // Геттеры объектов
    const QList<QSharedPointer<BaseObject>>& getObjects() const;
    QSharedPointer<BaseObject> getObjectAt(int index) const;
    int getObjectCount() const;

    // Геттеры свойств проекта
    QString getProjectName() const;
    int getCanvasWidth() const;
    int getCanvasHeight() const;
    QColor getBackgroundColor() const;
    QString getFilePath() const;

signals:
    void projectLoaded();                    // Проект загружен
    void projectChanged();                   // Проект изменён
    void logMessage(const QString &message); // Сообщение для лога

private:
    ProjectManager();
    
    QString m_projectName;   // Имя проекта
    int m_canvasWidth;       // Ширина холста
    int m_canvasHeight;      // Высота холста
    QColor m_bgColor;        // Цвет фона
    QString m_filePath;      // Путь к файлу

    QList<QSharedPointer<BaseObject>> m_objects;  // Список объектов
};
