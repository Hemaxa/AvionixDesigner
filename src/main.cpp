/**
 * @file main.cpp
 * @brief Точка входа в приложение XML-Editor
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
 * 
 * Порядок инициализации:
 * 1. Создание QApplication
 * 2. Регистрация типов объектов
 * 3. Загрузка стилей (опционально из файла)
 * 4. Создание и показ главного окна
 * 5. Запуск цикла событий
 */
int main(int argc, char *argv[])
{
    // 1. Создаем приложение Qt
    QApplication app(argc, argv);
    
    // Мета-информация приложения
    app.setApplicationName("XML Editor");
    app.setOrganizationName("MyCompany");
    app.setApplicationVersion("1.0");
    
    // 2. Регистрируем стандартные типы объектов
    //    (rectangle, rotationobject, staticgroup)
    ProjectManager::instance()->registerStandardTypes();
    
    // 3. Загружаем стили
    //    Можно загрузить из файла: AppearanceManager::instance()->loadStyleSheet("...");
    //    Или использовать встроенную тему (делается в MainWindow)
    
    // 4. Создаем и показываем главное окно
    MainWindow mainWindow;
    mainWindow.show();
    
    // 5. Запускаем цикл событий
    return app.exec();
}
