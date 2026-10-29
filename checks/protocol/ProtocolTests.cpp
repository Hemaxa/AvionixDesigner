// ProtocolTests — сравнение формата ПЛИС, движения, клавиш, компоновки и UART-сеанса.
#include "AircraftPositionPanel.h"
#include "ApplicationLanguage.h"
#include "EditorWorkspacePanel.h"
#include "ViewportPanel.h"
#include "ImageObject.h"
#include "RotationObject.h"
#include "StaticGroupObject.h"
#include "RectangleObject.h"
#include "fpga/FpgaProjectPacketBuilder.h"
#include <QColorDialog>
#include <QMenuBar>
#include <QMenu>
#include "AppearanceManager.h"
#include "ApplicationSettings.h"
#include "FlowLayout.h"
#include "BitParser.h"
#include "FpgaSchemaRegistry.h"
#include <QDomDocument>
#include "ProjectManager.h"
#include "ThreeDViewObject.h"
#include "ObjectLibraryPanel.h"
#include "ObjectListPanel.h"
#include "ObjectPropertiesPanel.h"
#include "FpgaSimulatorPanel.h"
#include "ViewportSettingsPanel.h"
#include "FpgaStreamingPanel.h"
#include "SelectionToolStrip.h"
#include "SettingsWindow.h"
#include "FontExportDialog.h"
#include "NewProjectDialog.h"
#include "MainWindow.h"
#include "fpga/model/Layer3dFormat.h"
#include "fpga/packets/FpgaPacketCompiler.h"
#include "fpga/packets/FpgaPacketFramer.h"
#include "fpga/transport/FpgaUartTransport.h"
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QTimer>
#include <QDockWidget>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QToolButton>
#include <QtTest>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <unistd.h>
#ifdef Q_OS_MACOS
#include <util.h>
#else
#include <pty.h>
#endif
#endif

#ifdef Q_OS_UNIX
struct PtyCapture {
    int master=-1,slave=-1;
    char name[256]{};
    std::atomic<bool> done{false};
    std::mutex mutex;
    QByteArray bytes;
    std::thread reader;
    PtyCapture() {
        if(openpty(&master,&slave,name,nullptr,nullptr)) return;
        ::close(slave); slave=-1;
        fcntl(master,F_SETFL,fcntl(master,F_GETFL)|O_NONBLOCK);
        reader=std::thread([this](){
            char buffer[4096];
            while(!done) {
                const auto count=::read(master,buffer,sizeof(buffer));
                if(count>0) { const std::lock_guard<std::mutex> lock(mutex); bytes.append(buffer,count); }
                else std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    QByteArray data() { const std::lock_guard<std::mutex> lock(mutex); return bytes; }
    ~PtyCapture() { done=true;if(reader.joinable()) reader.join();if(master>=0)::close(master); }
};
#endif

class ProtocolTests : public QObject
{
    Q_OBJECT
    QTemporaryDir m_settings;
private slots:
    void initTestCase() {
        // Проверки окон не читают и не перезаписывают пользовательские настройки.
        qputenv("AVIONIX_SETTINGS_FILE",m_settings.filePath("designer.ini").toUtf8());
        auto location=ApplicationSettings::create();
        QVERIFY2(location.fileName().startsWith(m_settings.path()),qPrintable(location.fileName()));
        QVERIFY(installRussianTranslation(*qApp));
        ProjectManager::instance()->registerStandardTypes();
        AppearanceManager::instance()->applyAvionixTheme();
    }
    void pythonVectors() {
        QFile file(QString::fromUtf8(PROTOCOL_VECTOR_FILE));
        QVERIFY2(file.open(QIODevice::ReadOnly), "Run packet_reference first to generate the Python vectors.");
        const auto vectors = QJsonDocument::fromJson(file.readAll()).array();
        QVERIFY(vectors.size() >= 1000);
        for (const auto &value : vectors) {
            const auto v = value.toObject();
            const quint32 course = Layer3dFormat::courseToRaw(v["course"].toDouble());
            const quint32 height = Layer3dFormat::heightToRaw(v["height"].toDouble());
            QCOMPARE(course, quint32(v["course_raw"].toInteger()));
            QCOMPARE(height, quint32(v["height_raw"].toInteger()));
            QString hex(38, '0');
            BitParser::inject(hex, 0, 19, v["x"].toInteger());
            BitParser::inject(hex, 19, 19, v["y"].toInteger());
            BitParser::inject(hex, 38, 19, course);
            BitParser::inject(hex, 57, 17, height);
            BitParser::inject(hex, 74, 10, v["yscreen"].toInt());
            const auto packet = FpgaPacketCompiler::layer3dPositionPacket(hex);
            QCOMPARE(packet.bytes.mid(7,19).toHex(), v["payload"].toString().toLatin1());
            QCOMPARE(packet.bytes.toHex(), v["packet"].toString().toLatin1());
        }
    }
    void modelAndHistory() {
        auto *p = ProjectManager::instance();
        QVERIFY(p->createNewProject("3D test",1920,1080,Qt::black));
        QVERIFY(p->addObject("layer3d") >= 0);
        auto *view = p->threeDViewObject();
        QCOMPARE(view->aircraftScreenY(),1023);
        const auto revision=p->sceneRevision();
        QVERIFY(p->setAircraftState(1.015625,2048.5,8000.25,45.05,123));
        QCOMPARE(p->sceneRevision(),revision);
        QCOMPARE(view->aircraftX(),1.015625);
        QCOMPARE(view->aircraftY(),2048.5);
        QCOMPARE(view->aircraftAltitude(),8000.25);
        QCOMPARE(view->aircraftScreenY(),123);
        const QString expected=view->serializeInit();
        QVERIFY(p->undo()); QCOMPARE(p->threeDViewObject()->aircraftScreenY(),1023);
        QVERIFY(p->redo()); QCOMPARE(p->threeDViewObject()->serializeInit(),expected);
        QTemporaryDir temp;
        QVERIFY(p->saveToFile(temp.filePath("scene.xml")));
        QVERIFY(p->loadFromFile(temp.filePath("scene.xml")));
        QCOMPARE(p->threeDViewObject()->serializeInit(),expected);
        p->setAircraftState(-1,99999,-99999,360,99999);
        QCOMPARE(p->threeDViewObject()->aircraftX(),0.);
        QCOMPARE(p->threeDViewObject()->aircraftY(),8191.);
        QCOMPARE(p->threeDViewObject()->aircraftAltitude(),-16384.);
        QCOMPARE(p->threeDViewObject()->aircraftScreenY(),1023);
        auto *restored=p->threeDViewObject();
        QString reserved=restored->serializeInit();
        BitParser::inject(reserved,84,32,0x89ABCDEF);
        BitParser::inject(reserved,116,32,0xFEDCBA98);
        BitParser::inject(reserved,148,2,3);
        restored->parse(reserved,FpgaSchemaRegistry::instance()->buildSchema("layer3d"));
        QVERIFY(p->setAircraftState(1234.25,5678.5,16000.75,359.95,1022));
        const QString withReserved=restored->serializeInit();
        QCOMPARE(withReserved.left(17),reserved.left(17));
        QVERIFY(p->saveToFile(temp.filePath("reserved.xml")));
        QVERIFY(p->loadFromFile(temp.filePath("reserved.xml")));
        QCOMPARE(p->threeDViewObject()->serializeInit(),withReserved);
    }
    void strictXmlAndEditorFlags() {
        auto *p = ProjectManager::instance();
        QVERIFY(p->createNewProject("Strict XML", 640, 480, Qt::black));
        for (const auto &type : {"aviahorizont", "dashline", "rectangle", "rectangle_a", "RibonScale", "staticgroup", "rotationobject", "layer3d", "text"})
            QVERIFY(p->addObject(type) >= 0);
        const auto baseline = p->buildCompiledFpgaXml();
        QString error;
        QVERIFY2(FpgaSchemaRegistry::instance()->validateXml(baseline, &error), qPrintable(error));
        const auto compiled = FpgaCompiledDocument::fromDomDocument(baseline, &error);
        QVERIFY2(compiled.isValid(), qPrintable(error));
        const int objectCount = p->getObjectCount();
        const int group = p->groupObjects({0, 1, 1, 2});
        QVERIFY(group >= 0);
        QCOMPARE(p->groupMembers(group).size(), 3);
        QVERIFY(p->setGroupSelectionLocked(group, true));
        QVERIFY(p->setGroupViewVisible(group, false));
        for (int i = 0; i < objectCount; ++i) {
            p->setObjectViewVisible(i, false);
            p->setObjectSelectionLocked(i, true);
        }
        // Группы, видимость и замочки не меняют даже биты init.
        QCOMPARE(p->buildCompiledFpgaXml().toByteArray(), baseline.toByteArray());
        p->threeDViewObject()->setLeftAnchored(true);
        QCOMPARE(p->buildCompiledFpgaXml().toByteArray(), baseline.toByteArray());
        const auto bundle = FpgaProjectPacketBuilder::buildCurrentProject(&error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(bundle.document.objects.first().leftAnchored);
        QTemporaryDir temp;
        QVERIFY(p->saveToFile(temp.filePath("scene.xml")));
        QVERIFY2(p->loadFromFile(temp.filePath("scene.xml")), qPrintable(p->lastErrorMessage()));
        QCOMPARE(p->getObjectCount(), objectCount);
        QVERIFY(p->objectGroups().isEmpty());
        for (const auto &object : p->getObjects()) {
            QVERIFY(object->isViewVisible());
            QVERIFY(!object->isSelectionLocked());
        }
        QVERIFY(!p->threeDViewObject()->isLeftAnchored());
        const auto reloaded = FpgaCompiledDocument::fromDomDocument(p->buildCompiledFpgaXml(), &error);
        QCOMPARE(reloaded.objects.size(), compiled.objects.size());
        for (int i = 0; i < compiled.objects.size(); ++i) {
            // Порядок пар кернинга в XML не меняет их смысл.
            QCOMPARE(reloaded.objects[i].type, compiled.objects[i].type);
            QCOMPARE(reloaded.objects[i].iparams, compiled.objects[i].iparams);
            QCOMPARE(reloaded.objects[i].data, compiled.objects[i].data);
            QCOMPARE(reloaded.objects[i].kerningPairs, compiled.objects[i].kerningPairs);
        }
        const QList<QPair<QString, QString>> oldNames = {
            {"aviahorizont", "aviagorizont"}, {"aviahorizont", "aviaGorizont"},
            {"dashline", "dashed_line"}, {"dashline", "DASHED_line"},
            {"rotationobject", "rotation_object"}, {"staticgroup", "static_group"},
            {"RibonScale", "ribonscale"}, {"layer3d", "three_d_view"}
        };
        for (const auto &names : oldNames) {
            auto xml = baseline.toString();
            xml.replace(names.first, names.second);
            QDomDocument invalid; QVERIFY(invalid.setContent(xml));
            QVERIFY(!FpgaSchemaRegistry::instance()->validateXml(invalid, &error));
            QVERIFY(!error.isEmpty());
            QVERIFY(!FpgaCompiledDocument::fromDomDocument(invalid, &error).isValid());
            QFile file(temp.filePath("invalid.xml")); QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(xml.toUtf8()); file.close();
            QVERIFY(!p->loadFromFile(file.fileName()));
            QCOMPARE(p->getObjectCount(), objectCount); // Не стираем открытый проект при отказе.
            QVERIFY(p->addObject(names.second) < 0);
        }
        auto oldSchema = baseline.toString(); oldSchema.replace("yscreen", "pitch");
        QDomDocument invalid; QVERIFY(invalid.setContent(oldSchema));
        QVERIFY(!FpgaSchemaRegistry::instance()->validateXml(invalid, &error));
    }
    void imageKindsAndMasks() {
        auto *p = ProjectManager::instance();
        QTemporaryDir temp;
        QImage image(16, 16, QImage::Format_ARGB32); image.fill(Qt::white);
        const QString png = temp.filePath("mask.png"); QVERIFY(image.save(png));
        const QString svg = temp.filePath("mask.svg");
        QFile source(svg); QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("<svg xmlns='http://www.w3.org/2000/svg' width='16' height='16'><rect width='16' height='16' fill='white'/></svg>"); source.close();
        for (const QString &path : {png, svg}) {
            QVERIFY(p->createNewProject("Images", 640, 480, Qt::black));
            QVERIFY(p->importImage(path, false) >= 0);
            QVERIFY(p->importImage(path, true) >= 0);
            auto *rotating = dynamic_cast<ImageObject *>(p->getObjectAt(0).data());
            auto *fixed = dynamic_cast<ImageObject *>(p->getObjectAt(1).data());
            QVERIFY(rotating && fixed);
            QCOMPARE(rotating->rotationDegrees, 0.0);
            QVERIFY(rotating->supportsRotationHandle()); QVERIFY(!fixed->supportsRotationHandle());
            fixed->setRotation(45); QCOMPARE(fixed->rotationDegrees, 0.0);
            QVERIFY(!fixed->setObjectProperty("Поворот (°)", "45"));
            auto dom = p->buildCompiledFpgaXml();
            QCOMPARE(dom.elementsByTagName("rotationobject").size(), 2); // схема и объект
            QCOMPARE(dom.elementsByTagName("staticgroup").size(), 2);
            const auto mask = dom.documentElement().firstChildElement("objects").firstChildElement("rotationobject").firstChildElement("data").text();
            for (int angle : {0, 90, 180, 270, 360}) {
                rotating->setRotation(angle);
                dom = p->buildCompiledFpgaXml();
                const auto element = dom.documentElement().firstChildElement("objects").firstChildElement("rotationobject");
                QCOMPARE(element.firstChildElement("data").text(), mask);
                const QString hex = element.firstChildElement("init").text();
                QCOMPARE(BitParser::extractSigned(hex, 105, 18), qRound(qSin(qDegreesToRadians(double(angle))) * 65536));
                QCOMPARE(BitParser::extractSigned(hex, 123, 18), qRound(qCos(qDegreesToRadians(double(angle))) * 65536));
            }
            const auto baseline = p->buildCompiledFpgaXml().toByteArray();
            p->groupObjects({0, 1}); p->setGroupViewVisible(p->objectGroups().first().id, false);
            QCOMPARE(p->buildCompiledFpgaXml().toByteArray(), baseline);
            QVERIFY(p->copyObjects({0, 1})); QCOMPARE(p->pasteObjects().size(), 2);
            QVERIFY(dynamic_cast<ImageObject *>(p->getObjectAt(0).data())->isRotationObject());
            QVERIFY(!dynamic_cast<ImageObject *>(p->getObjectAt(1).data())->isRotationObject());
            QVERIFY(p->undo()); QVERIFY(p->redo());
            QVERIFY(p->saveToFile(temp.filePath("images.xml")));
            QVERIFY2(p->loadFromFile(temp.filePath("images.xml")), qPrintable(p->lastErrorMessage()));
            QCOMPARE(p->getObjectCount(), 4);
            QVERIFY(dynamic_cast<RotationObject *>(p->getObjectAt(0).data()));
            QVERIFY(dynamic_cast<StaticGroupObject *>(p->getObjectAt(1).data()));
        }
    }
    void selectionAndLocks() {
        auto *p = ProjectManager::instance();
        p->createNewProject("Selection", 640, 480, Qt::black);
        p->addObject("aviahorizont"); p->addObject("rectangle"); p->addObject("rectangle");
        auto *a = dynamic_cast<RectangleObject *>(p->getObjectAt(0).data());
        auto *b = dynamic_cast<RectangleObject *>(p->getObjectAt(1).data());
        a->x = 60; a->y = 70; a->width = 80; a->height = 60;
        b->x = 280; b->y = 70; b->width = 80; b->height = 60;
        ViewportPanel viewport; viewport.resize(640,480); viewport.show(); QTest::qWait(5);
        auto point = [](double x, double y) { return QPoint(qRound(16 + x * 0.95), qRound(12 + y * 0.95)); };
        QObject::connect(&viewport, &ViewportPanel::selectionChanged, &viewport, &ViewportPanel::setSelectedIndexes);
        QTest::mouseClick(&viewport, Qt::LeftButton, Qt::NoModifier, point(100,100));
        QCOMPARE(viewport.getSelectedIndexes(), QList<int>{0});
        QTest::mouseClick(&viewport, Qt::LeftButton, Qt::ShiftModifier, point(320,100));
        QCOMPARE(viewport.getSelectedIndexes(), (QList<int>{0,1}));
        QTest::mouseClick(&viewport, Qt::LeftButton, Qt::ControlModifier, point(100,100));
        QCOMPARE(viewport.getSelectedIndexes(), QList<int>{1});
        QVERIFY(p->setObjectSelectionLocked(1, true)); QVERIFY(viewport.getSelectedIndexes().isEmpty());
        QVERIFY(p->setObjectSelectionLocked(2, true)); // Фон нельзя захватить рамкой.
        QTest::mousePress(&viewport, Qt::LeftButton, Qt::NoModifier, point(30,30));
        QTest::mouseMove(&viewport, point(400,200));
        QTest::mouseRelease(&viewport, Qt::LeftButton, Qt::NoModifier, point(400,200));
        QCOMPARE(viewport.getSelectedIndexes(), QList<int>{0});
        const int group = p->groupObjects({0,1}); QVERIFY(group >= 0);
        viewport.setSelectedIndexes({});
        QTest::mouseClick(&viewport, Qt::LeftButton, Qt::NoModifier, point(100,100));
        QCOMPARE(viewport.getSelectedIndexes(), QList<int>{0}); // Замок члена группы учитывается.
        ObjectListPanel list; list.refreshList(); list.show(); QTest::qWait(1);
        int locks = 0;
        for (auto *button : list.findChildren<QToolButton *>()) if (button->property("selectionLock").toBool()) ++locks;
        QCOMPARE(locks, 4); // Три объекта и группа.
        QVERIFY(p->setGroupSelectionLocked(group, true)); QVERIFY(viewport.getSelectedIndexes().isEmpty());
        QVERIFY(p->undo()); QVERIFY(!p->getObjectAt(0)->isSelectionLocked()); QVERIFY(p->getObjectAt(1)->isSelectionLocked());
        QVERIFY(p->redo()); QVERIFY(p->getObjectAt(0)->isSelectionLocked());
        list.refreshList(); list.selectRows({0,1,2});
        QVERIFY(list.findChild<QListWidget *>()->selectedItems().isEmpty());
        auto *widget = list.findChild<QListWidget *>();
        for (auto *button : widget->itemWidget(widget->item(0))->findChildren<QToolButton *>()) {
            if (button->property("selectionLock").toBool()) { button->click(); break; }
        }
        QVERIFY(!p->getObjectAt(0)->isSelectionLocked()); QVERIFY(!p->getObjectAt(1)->isSelectionLocked());
    }
    void toolbarAndRussianDialogs() {
        EditorWorkspacePanel workspace; workspace.resize(760, 360); workspace.show(); QTest::qWait(5);
        auto *strip = workspace.selectionToolStrip();
        QCOMPARE(strip->geometry().bottom(), workspace.viewport()->geometry().bottom());
        QCOMPARE(strip->height(), workspace.viewport()->height());
        auto *scroll = strip->findChild<QScrollArea *>(); QVERIFY(scroll);
        QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
        workspace.resize(760, 1400); QTest::qWait(5);
        QCOMPARE(strip->height(), workspace.viewport()->height());
        QCOMPARE(scroll->verticalScrollBar()->maximum(), 0);
        QColorDialog color; color.setOption(QColorDialog::DontUseNativeDialog); color.show(); QTest::qWait(5);
        bool russian = false;
        for (auto *label : color.findChildren<QLabel *>()) {
            russian = russian || label->text().contains(QStringLiteral("цвет"), Qt::CaseInsensitive);
            QVERIFY(!label->text().contains("Basic colors"));
        }
        QVERIFY(russian);
        color.grab().save(QCoreApplication::applicationDirPath()+"/russian-colors.png");
        MainWindow window;
        auto menus = window.menuBar()->findChildren<QMenu *>(QString(), Qt::FindDirectChildrenOnly);
        int imports = 0;
        for (auto *menu : menus) for (auto *action : menu->actions()) {
            if (action->text().contains(QStringLiteral("изображение"))) {
                QCOMPARE(menu->title(), QStringLiteral("Объекты")); ++imports;
            }
        }
        QCOMPARE(imports, 2);
    }
    void aircraftPivotAtEdges() {
        auto *p = ProjectManager::instance();
        p->createNewProject("Pivot", 1024,768,Qt::black); p->addObject("layer3d");
        AircraftPositionPanel panel; panel.resize(1030,438); panel.show(); QTest::qWait(5);
        auto *map = panel.findChild<QWidget *>("AircraftPositionScreen"); QVERIFY(map);
        for (double x : {0.,8191.}) for (double y : {0.,8191.}) {
            QPointF center;
            bool first = true;
            for (double altitude : {0.,16000.}) for (double course : {0.,90.,180.,270.}) {
                p->setAircraftState(x,y,altitude,course,1023); QTest::qWait(1);
                const auto image = map->grab().toImage();
                QRect bounds;
                for (int row=0;row<image.height();++row) for (int col=0;col<image.width();++col) {
                    const QColor c = image.pixelColor(col,row);
                    if (c.red()<25 && c.green()>175 && c.blue()>210)
                        bounds = bounds.united(QRect(col,row,1,1));
                }
                QVERIFY(!bounds.isEmpty());
                QVERIFY(bounds.left()>0 && bounds.top()>0 && bounds.right()<image.width()-1 && bounds.bottom()<image.height()-1);
                if (first) { center=bounds.center(); first=false; }
                QVERIFY2(QLineF(center,bounds.center()).length()<2.5, "Aircraft pivot moved when course or altitude changed");
            }
        }
        panel.grab().save(QCoreApplication::applicationDirPath()+"/aircraft-edge.png");
    }
    void invalidSerialConfig() {
        FpgaUartTransport sender;FpgaUartConfig config;QString error;
        QVERIFY(!sender.startStreaming({},config,&error));QVERIFY(!error.isEmpty());
        config.portName="AvionixNonexistentSerialPort";
        QSignalSpy failed(&sender,&FpgaUartTransport::failed);
        QVERIFY(sender.startStreaming({},config,&error));
        QTRY_COMPARE_WITH_TIMEOUT(failed.count(),1,3000);
        sender.stopStreaming();QVERIFY(!sender.isRunning());
    }
    void panelAndFlight() {
        auto *p=ProjectManager::instance();p->createNewProject("flight",1920,1080,Qt::black);p->addObject("layer3d");
        AircraftPositionPanel panel;
        auto *x=panel.findChild<QDoubleSpinBox *>("AircraftXSpin");
        auto *y=panel.findChild<QDoubleSpinBox *>("AircraftYSpin");
        auto *course=panel.findChild<QDoubleSpinBox *>("AircraftCourseSpin");
        auto *height=panel.findChild<QDoubleSpinBox *>("AircraftAltitudeSpin");
        auto *screen=panel.findChild<QSpinBox *>("AircraftScreenYSpin");
        auto *autoButton=panel.findChild<QToolButton *>("AircraftAutopilotButton");
        QVERIFY(x && y && course && height && screen && autoButton);
        QCOMPARE(screen->value(),1023);QCOMPARE(screen->minimum(),0);QCOMPARE(screen->maximum(),1023);
        for (const auto &size:{QSize(1030,438),QSize(800,360),QSize(1230,650),QSize(1030,370),QSize(1030,420),QSize(1030,480),QSize(800,900),QSize(1600,900)}) {
            panel.resize(size);panel.show();QTest::qWait(1);
            auto *map=panel.findChild<QWidget *>("AircraftPositionScreen");
            QCOMPARE(map->width(),map->height());
            QList<QWidget *> controls={map,x,y,course,height,screen,autoButton,
                panel.findChild<QWidget *>("AircraftCourseKnob"),panel.findChild<QWidget *>("AircraftAltitudeSlider")};
            for(auto *b:panel.findChildren<QToolButton *>("AircraftScreenYButton")) controls.append(b);
            QList<QRect> rectangles;
            for(auto *w:controls) {
                QVERIFY(w);QRect r(w->mapTo(&panel,QPoint()),w->size());QVERIFY(panel.rect().contains(r));
                for(const auto &other:rectangles) QVERIFY2(!r.intersects(other),qPrintable(w->objectName()));
                rectangles.append(r);
            }
            int captionY=-1;
            for(auto *label:panel.findChildren<QLabel *>("AircraftPositionCaption")) {
                if(label->text()=="Координаты" || label->text()=="Тангаж" || label->text()=="Автопилот") {
                    int top=label->mapTo(&panel,QPoint()).y();QVERIFY(captionY<0 || top==captionY);captionY=top;
                }
            }
            if(size==QSize(1600,900) || size==QSize(800,900))
                panel.grab().save(QCoreApplication::applicationDirPath()+QStringLiteral("/panel-%1x%2.png").arg(size.width()).arg(size.height()));
        }
        x->setValue(100);y->setValue(100);course->setValue(0);
        const auto revision=p->sceneRevision();
        autoButton->click();QTest::qWait(160);autoButton->click();
        const double distance=p->threeDViewObject()->aircraftX()-100;
        QVERIFY(distance>0.5 && distance<5); // Скорость зависит от прошедшего времени, а не числа тиков интерфейса.
        QCOMPARE(p->threeDViewObject()->aircraftY(),100.);
        QCOMPARE(p->sceneRevision(),revision);
        QVERIFY(std::fmod(p->threeDViewObject()->aircraftX(),1.)!=0.); // Дробные части координат сохраняются.
        x->setValue(0);y->setValue(100);course->setValue(90);
        autoButton->click();QTest::qWait(80);
        QVERIFY(autoButton->isChecked());QCOMPARE(p->threeDViewObject()->aircraftX(),0.);
        autoButton->click();
        panel.resize(1030,438);QTest::qWait(1);
        panel.grab().save(QCoreApplication::applicationDirPath()+"/protocol-panel.png");
    }
    void wrappingFlight() {
        auto *p=ProjectManager::instance();
        p->createNewProject("wrap",1024,768,Qt::black);p->addObject("layer3d");
        AircraftPositionPanel panel;panel.show();
        auto *button=panel.findChild<QToolButton *>("AircraftAutopilotButton");
        for(int course:{0,90,180,270,45}) {
            const double x=course==0 || course==45 ? 8191 : course==180 ? 0 : 4096;
            const double y=course==90 || course==45 ? 8191 : course==270 ? 0 : 4096;
            p->setAircraftState(x,y,1000.25,course,500);
            button->click();QTest::qWait(90);
            QVERIFY2(button->isChecked(),"Crossing the map edge must keep autopilot active.");
            auto *view=p->threeDViewObject();
            if(course==0 || course==45) QVERIFY(view->aircraftX()<5);
            if(course==90 || course==45) QVERIFY(view->aircraftY()<5);
            if(course==180) QVERIFY(view->aircraftX()>8186);
            if(course==270) QVERIFY(view->aircraftY()>8186);
            button->click();
        }
        QCOMPARE(Layer3dFormat::wrappedPosition(-0.25),8190.75);
        QCOMPARE(Layer3dFormat::wrappedPosition(8191.25),0.25);
        QCOMPARE(Layer3dFormat::wrappedPosition(8191.*3+0.5),0.5);
        QCOMPARE(Layer3dFormat::wrappedPosition(-8191.*3-0.5),8190.5);
    }
    void aircraftKeys() {
        auto *p=ProjectManager::instance();
        p->createNewProject("keys",1024,768,Qt::black);p->addObject("layer3d");
        p->setAircraftState(100,100,1000,45,500);
        AircraftPositionPanel panel;panel.show();panel.activateWindow();
        auto *map=panel.findChild<QWidget *>("AircraftPositionScreen");map->setFocus();
        QTest::keyClick(map,Qt::Key_Left);
        QVERIFY(qAbs(p->threeDViewObject()->aircraftCourse()-44.95)<0.001);
        QTest::keyClick(map,Qt::Key_Right,Qt::ShiftModifier);
        QVERIFY(qAbs(p->threeDViewObject()->aircraftCourse()-45.95)<0.001);
        QTest::keyClick(map,Qt::Key_Up);QCOMPARE(p->threeDViewObject()->aircraftScreenY(),501);
        QTest::keyClick(map,Qt::Key_Down,Qt::ShiftModifier);QCOMPARE(p->threeDViewObject()->aircraftScreenY(),491);
        QTest::keyClick(map,Qt::Key_Plus);QCOMPARE(p->threeDViewObject()->aircraftAltitude(),1000.25);
        QTest::keyClick(map,Qt::Key_Minus,Qt::ShiftModifier);QCOMPARE(p->threeDViewObject()->aircraftAltitude(),990.25);
        QTest::keyClick(map,Qt::Key_Equal);QCOMPARE(p->threeDViewObject()->aircraftAltitude(),990.5);
        auto *x=panel.findChild<QDoubleSpinBox *>("AircraftXSpin");x->setFocus();
        x->selectAll();QTest::keyClicks(x,"123");QTest::keyClick(x,Qt::Key_Return);
        QCOMPARE(p->threeDViewObject()->aircraftX(),123.);
        QCOMPARE(p->threeDViewObject()->aircraftAltitude(),990.5);
        const double course=p->threeDViewObject()->aircraftCourse();
        QTest::keyClick(x,Qt::Key_Left);QCOMPARE(p->threeDViewObject()->aircraftCourse(),course);
        QTest::keyClick(x,Qt::Key_Plus);QTest::keyClick(x,Qt::Key_Equal);
        QCOMPARE(p->threeDViewObject()->aircraftAltitude(),990.5);
    }
    void adaptivePanels() {
        ObjectLibraryPanel library;ViewportSettingsPanel canvas;FpgaStreamingPanel fpga;
        SelectionToolStrip strip;
        for(auto *panel:{static_cast<QWidget *>(&library),static_cast<QWidget *>(&canvas),static_cast<QWidget *>(&fpga),static_cast<QWidget *>(&strip)}) {
            for(const QSize size:{QSize(128,420),QSize(320,96),QSize(900,90)}) {
                panel->resize(size);panel->show();QTest::qWait(5);
                auto *scroll=panel->findChild<QScrollArea *>();QVERIFY(scroll);
                QVERIFY(scroll->horizontalScrollBar()->maximum()==0);
                QList<QRect> occupied;
                for(auto *widget:scroll->widget()->findChildren<QWidget *>()) {
                    if(!qobject_cast<QToolButton *>(widget) && !qobject_cast<QSpinBox *>(widget)) continue;
                    const QRect rect(widget->mapTo(scroll->widget(),QPoint()),widget->size());
                    QVERIFY2(scroll->widget()->rect().contains(rect),qPrintable(panel->objectName()));
                    for(const auto &other:occupied) QVERIFY(!rect.intersects(other));
                    occupied.append(rect);
                }
            }
        }
    }
    void dialogsAndWorkspace() {
        SettingsWindow settings;NewProjectDialog project;
        FontExportDialogEntry entry;entry.fontFamily="Arial";entry.pixelSize=18;
        FontExportDialog fonts({entry});
        for(auto *dialog:{static_cast<QDialog *>(&settings),static_cast<QDialog *>(&project),static_cast<QDialog *>(&fonts)}) {
            dialog->resize(dialog->minimumSize());dialog->show();QTest::qWait(5);
            for(auto *button:dialog->findChildren<QDialogButtonBox *>())
                QVERIFY(dialog->rect().contains(QRect(button->mapTo(dialog,QPoint()),button->size())));
            dialog->grab().save(QCoreApplication::applicationDirPath()+"/"+dialog->objectName()+".png");
            dialog->hide();
        }
        bool found=false;
        for(auto *label:settings.findChildren<QLabel *>()) if(label->text()=="Shift + ↑ / ↓") found=true;
        QVERIFY(found);
        MainWindow window;window.showNormal();window.resize(1100,720);QTest::qWait(10);
        QVERIFY(window.minimumSizeHint().height()<=720);
        for(auto *dock:window.findChildren<QDockWidget *>()) {
            if(dock->isFloating() || !dock->isVisible()) continue;
            QVERIFY(window.rect().contains(dock->geometry()));
            auto *scroll=dock->findChild<QScrollArea *>();
            if(scroll && scroll->widget() && dynamic_cast<FlowLayout *>(scroll->widget()->layout()))
                QCOMPARE(scroll->verticalScrollBar()->maximum(),0);
        }
        window.grab().save(QCoreApplication::applicationDirPath()+"/workspace.png");
    }
    void uartDialogLayout() {
        auto *p=ProjectManager::instance();
        p->createNewProject("uart dialog",1024,768,Qt::black);p->addObject("layer3d");
        FpgaStreamingPanel panel;
        bool inspected=false;QString issue;
        QTimer::singleShot(0,&panel,[&](){
            auto *dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if(!dialog) return;
            inspected=dialog->objectName()=="UartStreamingDialog";
            QList<QRect> rectangles;
            for(auto *widget:dialog->findChildren<QWidget *>()) {
                if(!qobject_cast<QAbstractSpinBox *>(widget) && !qobject_cast<QComboBox *>(widget)
                    && !qobject_cast<QPushButton *>(widget)) continue;
                QRect rect(widget->mapTo(dialog,QPoint()),widget->size());
                if(!dialog->rect().contains(rect)) issue="Control lies outside UART dialog.";
                for(auto other:rectangles) if(other.intersects(rect)) issue="UART controls overlap.";
                rectangles.append(rect);
            }
            dialog->grab().save(QCoreApplication::applicationDirPath()+"/uart-dialog.png");
            dialog->reject();
        });
        panel.toggleUartMode();panel.startStreaming();
        QVERIFY(inspected);QVERIFY2(issue.isEmpty(),qPrintable(issue));
    }
    void longLabelsAndWarnings() {
        auto *p=ProjectManager::instance();
        p->createNewProject("labels",1024,768,Qt::black);
        const int index=p->addObject("rectangle");QVERIFY(index>=0);
        p->getObjectAt(index)->setCustomName(QString(200,QLatin1Char('W')));
        ObjectListPanel list;list.refreshList();list.resize(220,220);list.show();QTest::qWait(5);
        for(auto *box:list.findChildren<QCheckBox *>())
            QVERIFY(list.rect().contains(QRect(box->mapTo(&list,QPoint()),box->size())));
        ObjectPropertiesPanel properties;properties.showObjectProperties(index);
        properties.resize(260,220);properties.show();QTest::qWait(5);
        FpgaSimulatorPanel simulator;
        FpgaPacketBundle bundle;bundle.document.width=320;bundle.document.height=240;
        for(int i=0;i<40;++i) bundle.warnings.append(QStringLiteral("Длинное сообщение симулятора %1").arg(i));
        simulator.loadBundle(bundle);simulator.resize(320,320);simulator.show();QTest::qWait(5);
        QVERIFY(simulator.minimumSizeHint().height()<=320);
        auto *warnings=simulator.findChild<QScrollArea *>();QVERIFY(warnings);
        QVERIFY(warnings->verticalScrollBar()->maximum()>0);
    }
    void serialSession() {
#ifndef Q_OS_UNIX
        QSKIP("Pseudo-terminal integration check runs on Unix; byte-level checks also run on Windows.");
#else
        PtyCapture capture;
        QVERIFY(capture.master>=0);
        FpgaPacketBundle bundle;
        FpgaPacket ordinary;ordinary.type=FpgaPacketType::Parameter;ordinary.payloadSize=19;
        ordinary.bytes=FpgaPacketFramer::frame(3,FpgaPacketCompiler::parameterPayload(0,QByteArray(19,'A')));
        bundle.packets={ordinary,FpgaPacketCompiler::layer3dSidePacket(true),FpgaPacketCompiler::layer3dPositionPacket(QString(38,'0'))};
        FpgaUartTransport sender;
        QSignalSpy failed(&sender,&FpgaUartTransport::failed);
        QSignalSpy setup(&sender,&FpgaUartTransport::setupComplete);
        FpgaUartConfig config;config.portName=QString::fromLocal8Bit(capture.name);config.loadBaudRate=57600;config.intervalMs=20;
        int requested=0;
        auto provider = [&](){
            ++requested;QString hex(38,'0');BitParser::inject(hex,0,19,requested);
            sender.updatePosition(FpgaPacketCompiler::layer3dPositionPacket(hex).bytes);
        };
        auto connection=connect(&sender,&FpgaUartTransport::positionRequested,this,provider);
        QString error;QVERIFY(sender.startStreaming(bundle,config,&error));
        QTRY_VERIFY_WITH_TIMEOUT(setup.count()>0 || failed.count()>0,3000);
        QVERIFY2(failed.isEmpty(),qPrintable(failed.isEmpty()?QString():failed.first().first().toString()));
        QTRY_VERIFY_WITH_TIMEOUT(requested>=6,1500);
        sender.updateScene(bundle); // Неизмененные маски не должны загружаться повторно.
        auto changed=bundle;
        changed.packets[1]=FpgaPacketCompiler::layer3dSidePacket(false);
        sender.updateScene(changed);
        QTRY_VERIFY_WITH_TIMEOUT(capture.data().contains(changed.packets[1].bytes),1000);
        disconnect(connection);
        QTest::qWait(50);
        for(int i=100;i<=999;++i) {
            QString hex(38,'0');BitParser::inject(hex,0,19,i);
            sender.updatePosition(FpgaPacketCompiler::layer3dPositionPacket(hex).bytes);
        }
        auto lastX=[&](){
            auto data=capture.data();int offset=1,result=-1;
            while(offset+11<=data.size()) {
                int size=6+(quint8(data[offset+4])<<8)+quint8(data[offset+5]);
                if(offset+size>data.size()) break;
                if(size==30 && quint8(data[offset+6])==6) {
                    auto payload=data.mid(offset+7,19);std::reverse(payload.begin(),payload.end());
                    result=BitParser::extract(QString::fromLatin1(payload.toHex()),0,19);
                }
                offset+=size;
            }
            return result;
        };
        QTRY_COMPARE_WITH_TIMEOUT(lastX(),999,1000);
        sender.stopStreaming();
        QTest::qWait(10);
        QByteArray bytes=capture.data();
        const auto prefix=QByteArray(1,char(0))+FpgaUartTransport::sceneBytes(bundle);
        QVERIFY(bytes.startsWith(prefix));bytes.remove(0,prefix.size());
        int poses=0,sides=0;
        while(!bytes.isEmpty()) {
            QVERIFY(bytes.startsWith("SAIN"));QVERIFY(bytes.size()>=11);
            const int size=6+(quint8(bytes[4])<<8)+quint8(bytes[5]);
            QVERIFY(bytes.size()>=size);QCOMPARE(quint8(bytes[6]),quint8(6));
            const auto packet=bytes.left(size);
            QCOMPARE(packet.right(4).toHex(),QByteArray::number(FpgaPacketFramer::fletcher32(packet.left(size-4)),16).rightJustified(8,'0'));
            if(size==12) { QCOMPARE(quint8(packet[7]),quint8(0));++sides; }
            else { QCOMPARE(size,30);++poses; }
            bytes.remove(0,size);
        }
        QVERIFY(poses>=5 && poses<20);QCOMPARE(sides,1);QVERIFY(failed.isEmpty());
        connection=connect(&sender,&FpgaUartTransport::positionRequested,this,provider);
        requested=0;config.intervalMs=0;
        QVERIFY(sender.startStreaming(bundle,config,&error));
        QTRY_VERIFY_WITH_TIMEOUT(requested>=20,1500);
        sender.stopStreaming();
#endif
    }
};
QTEST_MAIN(ProtocolTests)
#include "ProtocolTests.moc"
