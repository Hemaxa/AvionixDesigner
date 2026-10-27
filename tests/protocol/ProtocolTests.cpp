#include "AircraftPositionPanel.h"
#include "AppearanceManager.h"
#include "BitParser.h"
#include "FpgaSchemaRegistry.h"
#include <QDomDocument>
#include "ProjectManager.h"
#include "ThreeDViewObject.h"
#include "fpga/model/Layer3dFormat.h"
#include "fpga/packets/FpgaPacketCompiler.h"
#include "fpga/packets/FpgaPacketFramer.h"
#include "fpga/transport/FpgaUartTransport.h"
#include <QDoubleSpinBox>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
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
private slots:
    void initTestCase() {
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
        QVERIFY(p->addObject("three_d_view") >= 0);
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
    void legacyScreenField() {
        auto schema=FpgaSchemaRegistry::instance()->buildSchema("layer3d");
        schema.insert("pitch",schema.take("yscreen"));
        QString hex(38,'0');BitParser::inject(hex,74,10,512);
        ThreeDViewObject view;view.parse(hex,schema);
        QCOMPARE(view.aircraftScreenY(),512);
        ParamSchema origin;origin.insert("enb",{0,1});origin.insert("x0",{1,12});
        view.parse("1",origin);
        QDomDocument dom;auto element=dom.createElement("layer3d");view.parseExtraData(element);
        QVERIFY(view.isLeftAnchored());
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
        auto *p=ProjectManager::instance();p->createNewProject("flight",1920,1080,Qt::black);p->addObject("three_d_view");
        AircraftPositionPanel panel;
        auto *x=panel.findChild<QDoubleSpinBox *>("AircraftXSpin");
        auto *y=panel.findChild<QDoubleSpinBox *>("AircraftYSpin");
        auto *course=panel.findChild<QDoubleSpinBox *>("AircraftCourseSpin");
        auto *height=panel.findChild<QDoubleSpinBox *>("AircraftAltitudeSpin");
        auto *screen=panel.findChild<QSpinBox *>("AircraftScreenYSpin");
        auto *autoButton=panel.findChild<QToolButton *>("AircraftAutopilotButton");
        QVERIFY(x && y && course && height && screen && autoButton);
        QCOMPARE(screen->value(),1023);QCOMPARE(screen->minimum(),0);QCOMPARE(screen->maximum(),1023);
        for (const auto &size:{QSize(1030,438),QSize(800,360),QSize(1230,650),QSize(1030,370),QSize(1030,420),QSize(1030,480)}) {
            panel.resize(size);panel.show();QTest::qWait(1);
            QList<QWidget *> controls={x,y,course,height,screen,autoButton,
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
                if(label->text()=="Координаты" || label->text()=="Y экрана" || label->text()=="Автопилот") {
                    int top=label->mapTo(&panel,QPoint()).y();QVERIFY(captionY<0 || top==captionY);captionY=top;
                }
            }
        }
        x->setValue(100);y->setValue(100);course->setValue(0);
        const auto revision=p->sceneRevision();
        autoButton->click();QTest::qWait(160);autoButton->click();
        const double distance=p->threeDViewObject()->aircraftX()-100;
        QVERIFY(distance>0.5 && distance<5); // movement is based on elapsed time, not GUI tick count
        QCOMPARE(p->threeDViewObject()->aircraftY(),100.);
        QCOMPARE(p->sceneRevision(),revision);
        QVERIFY(std::fmod(p->threeDViewObject()->aircraftX(),1.)!=0.); // sub-point movement survives
        x->setValue(0);y->setValue(100);course->setValue(90);
        autoButton->click();QTest::qWait(80);
        QVERIFY(autoButton->isChecked());QCOMPARE(p->threeDViewObject()->aircraftX(),0.);
        autoButton->click();
        panel.resize(1030,438);QTest::qWait(1);
        panel.grab().save(QCoreApplication::applicationDirPath()+"/protocol-panel.png");
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
        sender.updateScene(bundle); // unchanged masks must not be re-uploaded
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
