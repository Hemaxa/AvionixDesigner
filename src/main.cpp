/**
 * @file main.cpp
 * @brief Точка входа в приложение Avionix Designer
 * 
 * Инициализирует:
 * - QApplication
 * - Менеджеры (ProjectManager, AppearanceManager, ObjectsManager)
 * - Главное окно
 */

#include <QApplication>
#include "managers/ProjectManager.h"
#include "managers/AppearanceManager.h"
#include "managers/ObjectsManager.h"
#include "windows/MainWindow.h"

/**
 * @brief Главная функция приложения
 */
int main(int argc, char *argv[])
{
    // 1. Создаем приложение Qt
    QApplication app(argc, argv);
    
    // Мета-информация приложения
    app.setApplicationName("Avionix Designer");
    app.setOrganizationName("Avionix");
    app.setApplicationVersion("1.0");
    
    // 2. Регистрируем стандартные типы объектов
    ProjectManager::instance()->registerStandardTypes();
    
    // 3. Создаем и показываем главное окно
    MainWindow mainWindow;
    mainWindow.show();
    
    // 4. Запускаем цикл событий
    return app.exec();
}
