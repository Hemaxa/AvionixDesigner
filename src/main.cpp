//main - точка входа в приложение

#include "managers/ProjectManager.h"
#include "managers/AppearanceManager.h"
#include "managers/ObjectsManager.h"
#include "windows/MainWindow.h"

#include <QApplication>

//главная функция приложения
int main(int argc, char *argv[])
{
    //создание приложения Qt
    QApplication app(argc, argv);
    
    //мета-информация о приложении
    app.setApplicationName("Avionix Designer");
    app.setOrganizationName("Avionix");
    app.setApplicationVersion("1.0");
    
    //регистрируем стандартные типы объектов (получаем словарь поддерживаемых объектов)
    ProjectManager::instance()->registerStandardTypes();
    
    //создаем и показываем главное окно
    MainWindow mainWindow;
    mainWindow.show();
    
    //запускаем бесконечный цикл событий
    return app.exec();
}
