//main - точка входа в приложение

#include "ProjectManager.h"
#include "AppearanceManager.h"
#include "ObjectsManager.h"
#include "MainWindow.h"
#include "ApplicationSettings.h"
#include "ApplicationLanguage.h"
#include "fpga/FpgaProjectPacketBuilder.h"
#include "fpga/transport/FpgaPacketDump.h"

#include <QApplication>
#include <QIcon>
#include <QString>
#include <QCommandLineParser>
#include <QTextStream>
#include <QTemporaryDir>
#include <memory>

#ifndef APP_VERSION
#define APP_VERSION "1.0.0"
#endif

//главная функция приложения
int main(int argc, char *argv[])
{
    bool commandLineExport = false;
    for (int i = 1; i < argc; ++i) {
        const QString argument = QString::fromLocal8Bit(argv[i]);
        if (argument == QStringLiteral("--export-bin") || argument.startsWith(QStringLiteral("--export-bin="))
            || argument == QStringLiteral("--export-xml") || argument.startsWith(QStringLiteral("--export-xml="))) {
            commandLineExport = true;
            if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) qputenv("QT_QPA_PLATFORM", "offscreen");
        }
    }
    //создание приложения Qt
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    if (!installRussianTranslation(app)) {
        QTextStream(stderr) << "Не удалось загрузить русский перевод Qt.\n";
        return 1;
    }
    std::unique_ptr<QTemporaryDir> exportSettings;
    if (commandLineExport && qEnvironmentVariableIsEmpty("AVIONIX_SETTINGS_FILE")) {
        exportSettings = std::make_unique<QTemporaryDir>();
        if (!exportSettings->isValid()) {
            QTextStream(stderr) << "Cannot create a temporary settings profile.\n";
            return 1;
        }
        qputenv("AVIONIX_SETTINGS_FILE",exportSettings->filePath("designer.ini").toUtf8());
    }
    
    //мета-информация о приложении
    app.setApplicationName("Avionix Designer");
    app.setOrganizationName("Avionix");
    app.setApplicationVersion(QString::fromLatin1(APP_VERSION));
    app.setWindowIcon(QIcon(QStringLiteral(":/app/icon.png")));
    
    //регистрируем стандартные типы объектов (получаем словарь поддерживаемых объектов)
    ProjectManager::instance()->registerStandardTypes();

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption binOption(QStringLiteral("export-bin"), QStringLiteral("Compile XML to BIN without opening the editor."), QStringLiteral("xml"));
    const QCommandLineOption xmlOption(QStringLiteral("export-xml"), QStringLiteral("Load and save a canonical FPGA XML without opening the editor."), QStringLiteral("xml"));
    parser.addOption(binOption);
    parser.addOption(xmlOption);
    parser.addPositionalArgument(QStringLiteral("output"), QStringLiteral("Output BIN or XML path."));
    parser.process(app);
    if (parser.isSet(binOption) || parser.isSet(xmlOption)) {
        if (parser.isSet(binOption) == parser.isSet(xmlOption) || parser.positionalArguments().size() != 1)
            parser.showHelp(2);
        auto *project = ProjectManager::instance();
        const QString input = parser.value(parser.isSet(binOption) ? binOption : xmlOption);
        if (!project->loadFromFile(input)) {
            QTextStream(stderr) << project->lastErrorMessage() << '\n';
            return 1;
        }
        const QString output = parser.positionalArguments().first();
        QString error;
        bool success;
        if (parser.isSet(binOption)) {
            const auto bundle = FpgaProjectPacketBuilder::buildCurrentProject(&error);
            success = error.isEmpty() && FpgaPacketDump::writeBinary(output, bundle, &error);
        } else {
            success = project->exportToFpgaXml(output);
            error = project->lastErrorMessage();
        }
        if (!success) QTextStream(stderr) << error << '\n';
        return success ? 0 : 1;
    }
    
    //создаем и показываем главное окно
    MainWindow mainWindow;
    mainWindow.show();
    
    //запускаем бесконечный цикл событий
    return app.exec();
}
