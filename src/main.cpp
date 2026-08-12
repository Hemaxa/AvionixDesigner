//main - точка входа в приложение

#include "ProjectManager.h"
#include "AppearanceManager.h"
#include "ObjectsManager.h"
#include "MainWindow.h"

#include <QApplication>
#include <QIcon>
#include <QString>

#ifndef APP_VERSION
#define APP_VERSION "1.0.0"
#endif

//главная функция приложения
int main(int argc, char *argv[])
{
    //создание приложения Qt
    QApplication app(argc, argv);
    
    //мета-информация о приложении
    app.setApplicationName("Avionix Designer");
    app.setOrganizationName("Avionix");
    app.setApplicationVersion(QString::fromLatin1(APP_VERSION));
    app.setWindowIcon(QIcon(QStringLiteral(":/app/icon.png")));
    
    //регистрируем стандартные типы объектов (получаем словарь поддерживаемых объектов)
    ProjectManager::instance()->registerStandardTypes();
    
    //создаем и показываем главное окно
    MainWindow mainWindow;
    mainWindow.show();
    
    //запускаем бесконечный цикл событий
    return app.exec();
}
