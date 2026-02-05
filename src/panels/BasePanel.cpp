#include "BasePanel.h"

BasePanel::BasePanel(const QString &objectName, QWidget *parent) : QWidget(parent)
{
    // Устанавливаем имя объекта для обращения через QSS
    setObjectName(objectName);
    
    // Базовые настройки для всех панелей
    setMinimumSize(100, 100);
    
    // Вызываем инициализацию UI
    setupUI();
}

BasePanel::~BasePanel() = default;

void BasePanel::setupUI()
{
    // Базовая реализация пустая
    // Дочерние классы переопределяют этот метод
}
