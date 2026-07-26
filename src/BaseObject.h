//BaseObject - базовый абстрактный класс для всех графических объектов
 
#pragma once

#include <QObject>
#include <QPainter>
#include <QMap>
#include <QString>
#include <QDomElement>

//структура одного конкретного параметра в битовой схеме
struct ParamInfo 
{
    int offset; //смещение в битах
    int size; //размер в битах
};

//тип для схемы параметров
using ParamSchema = QMap<QString, ParamInfo>;

class BaseObject : public QObject
{
    Q_OBJECT
    
public:
    //виртуальные методы должны быть переопределены в классах наследниках
    //объект знает, какие у него должны быть параметры, сам их читает и сам по ним себя отрисовывает

    explicit BaseObject(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~BaseObject() = default;
    
    //виртуальный метод, который парсит основные параметры из HEX-строки
    virtual void parse(const QString &hexInit, const ParamSchema &schema) = 0;
    
    //виртуальный метод, который парсит дополнительные данные из XML-элемента
    virtual void parseExtraData(const QDomElement &element) { Q_UNUSED(element); }
    
    //виртуальный метод, который отрисовывает объект на холсте
    virtual void draw(QPainter &painter) = 0;
    
    //виртуальные геттеры для свойств объекта
    virtual QString getTypeName() const = 0; //возвращает тип объекта
    virtual QString getDisplayName() const { return getTypeName(); } //отображаемое имя для UI
    virtual QList<QPair<QString, QString>> getProperties() const = 0; //возвращает список свойств объекта
    virtual QRectF getBoundingRect() const = 0;  //возвращает ограничивающий прямоугольник (занимаемое пространство)
    virtual bool supportsRotationHandle() const { return false; }
    virtual bool canResize() const { return !m_resizeLocked; }
    virtual bool isImportedHardwareObject() const { return m_importedHardwareObject; }
    virtual void setImportedHardwareObject(bool imported) { m_importedHardwareObject = imported; }
    virtual void setResizeLocked(bool locked) { m_resizeLocked = locked; }
    virtual bool isViewVisible() const { return m_viewVisible; }
    virtual void setViewVisible(bool visible) { m_viewVisible = visible; emit changed(); }
    virtual bool isExportEnabled() const { return m_exportEnabled; }
    virtual void setExportEnabled(bool enabled) { m_exportEnabled = enabled; emit changed(); }
    virtual QString editRestrictionHint() const { return m_editRestrictionHint; }
    virtual QString lastValidationMessage() const { return m_lastValidationMessage; }
    virtual void clearValidationMessage() { m_lastValidationMessage.clear(); }
    QString customName() const { return m_customName; }
    void setCustomName(const QString &name) { m_customName = name.trimmed(); }

    //геометрия и взаимодействие
    virtual bool contains(const QPointF &point) const { return getBoundingRect().contains(point); }
    virtual void moveBy(double dx, double dy) { Q_UNUSED(dx); Q_UNUSED(dy); }
    virtual void resizeBy(int edgeFlags, double dx, double dy) { Q_UNUSED(edgeFlags); Q_UNUSED(dx); Q_UNUSED(dy); }
    virtual void setRotation(double angle) { Q_UNUSED(angle); }

    
    //устанавливает значение свойства по имени, возвращает true при успехе
    virtual bool setObjectProperty(const QString &name, const QString &value) { Q_UNUSED(name); Q_UNUSED(value); return false; }

    //возвращает карту {имя_параметра_схемы → сырое значение} для сериализации в HEX
    virtual QMap<QString, quint32> serializeParams() const { return {}; }

signals:
    void changed();  //сигнал об изменении объекта

protected:
    void setEditRestrictionHint(const QString &hint) { m_editRestrictionHint = hint; }
    void setValidationMessage(const QString &message) const { m_lastValidationMessage = message; }

private:
    bool m_resizeLocked = false;
    bool m_importedHardwareObject = false;
    bool m_viewVisible = true;
    bool m_exportEnabled = true;
    QString m_customName;
    QString m_editRestrictionHint;
    mutable QString m_lastValidationMessage;
};
