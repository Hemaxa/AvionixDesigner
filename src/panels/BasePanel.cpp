#include "BasePanel.h"

BasePanel::BasePanel(const QString &objectName, QWidget *parent) : QWidget(parent)
{
    //устанавливаем имя объекта для обращения через QSS
    setObjectName(objectName);
    
    //базовые настройки для всех панелей
    setMinimumSize(100, 100);
    
    //вызываем инициализацию UI
    setupUI();
}

BasePanel::~BasePanel() = default;

void BasePanel::setupUI() {}
