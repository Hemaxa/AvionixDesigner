// ProtocolTests - сравнение формата ПЛИС, движения, клавиш, компоновки и UART-сеанса.
#include "AircraftPositionPanel.h"
#include "LayersPanel.h"
#include "ApplicationLanguage.h"
#include "EditorWorkspacePanel.h"
#include "ViewportPanel.h"
#include "ImageObject.h"
#include "RotationObject.h"
#include "StaticGroupObject.h"
#include "TextObject.h"
#include "text/TextLayout.h"
#include "text/FontRasterizer.h"
#include "fpga/compiler/FpgaSceneCompiler.h"
#include "fpga/compiler/FpgaResourceLimits.h"
#include "fpga/simulator/FpgaSimulator.h"
#include "fpga/transport/FpgaPacketDump.h"
#include "RectangleObject.h"
#include "DashedLineObject.h"
#include "RibbonScaleObject.h"
#include "AviaHorizonObject.h"
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
#include <QSlider>
#include <QTableWidget>
#include <QMouseEvent>
#include <QAction>
#include <QTemporaryDir>
#include <QToolButton>
#include <QtTest>
#include <atomic>
#include <memory>
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
// PtyCapture - перехват байтов виртуального последовательного порта в проверках UART.
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

namespace {
QString testInit(const QString &type, const QMap<QString, quint32> &values)
{
    const auto schema = FpgaSchemaRegistry::instance()->buildSchema(type);
    QString hex(38, '0');
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        const auto info = schema.value(it.key());
        BitParser::inject(hex, info.offset, info.size, it.value());
    }
    return hex;
}

QDomDocument textFixture(const QString &text = QStringLiteral("AVA A"), int x = 10, int y = 20)
{
    QDomDocument doc;
    auto root = doc.createElement("project");
    root.setAttribute("width", 80); root.setAttribute("height", 50); root.setAttribute("bgcolor", "#203040");
    doc.appendChild(root);
    auto params = doc.createElement("parameters"); root.appendChild(params);
    for (const auto &type : {"font", "text", "rectangle"}) {
        auto schema = doc.createElement(type); params.appendChild(schema);
        for (const auto &definition : FpgaSchemaRegistry::instance()->fieldsForSchema(type)) {
            auto field = doc.createElement(definition.name);
            field.setAttribute("offset", definition.offset); field.setAttribute("size", definition.size);
            schema.appendChild(field);
        }
    }
    auto objects = doc.createElement("objects"); root.appendChild(objects);
    auto font = doc.createElement("font"); font.setAttribute("index", 0); font.setAttribute("name", "Synthetic"); font.setAttribute("size", 14);
    objects.appendChild(font);
    auto glyph = [&](QChar ch, int w, int h, int advance, int bx, int by, int offset) {
        auto init = doc.createElement("init"); init.setAttribute("literal", QString(ch)); init.setAttribute("code", ch.unicode());
        init.appendChild(doc.createTextNode(testInit("font", {
            {"enb",1}, {"code",ch.unicode()}, {"w",quint32(w)}, {"h",quint32(h)}, {"advance",quint32(advance)},
            {"bearing_x",quint32(bx)}, {"bearing_y",quint32(by)}, {"offset",quint32(offset)}, {"mask_size",quint32(w*h)}
        })));
        font.appendChild(init);
    };
    glyph('A',3,2,5,-1,-3,0); glyph('V',2,3,4,1,-2,6); glyph(' ',1,1,3,0,0,12); glyph(QChar(0x042F),2,2,4,0,-3,13);
    auto data = doc.createElement("data"); data.appendChild(doc.createTextNode("7,0,7,3,7,0,7,6,5,0,2,7,0,7,7,1,7")); font.appendChild(data);
    for (const auto &pair : {QStringLiteral("AV"), QStringLiteral("VA")}) {
        auto kern = doc.createElement("kerning"); kern.setAttribute("left", QString(pair[0])); kern.setAttribute("right", QString(pair[1]));
        kern.setAttribute("delta", pair == "AV" ? -2 : -1); font.appendChild(kern);
    }
    auto object = doc.createElement("text"); objects.appendChild(object);
    auto init = doc.createElement("init"); init.appendChild(doc.createTextNode(testInit("text", {
        {"enb",1}, {"color",0x563412}, {"x",quint32(x)}, {"y",quint32(y)}, {"char_count",quint32(text.size())}
    }))); object.appendChild(init);
    QStringList codes;
    for (const auto ch : text) codes.append(QString::number(ch.unicode()));
    data = doc.createElement("data"); data.appendChild(doc.createTextNode(codes.join(','))); object.appendChild(data);
    return doc;
}

FpgaPacketBundle compileTextFixture(const QDomDocument &doc, QString *error)
{
    const auto source = FpgaCompiledDocument::fromDomDocument(doc, error);
    if (!source.isValid()) return {};
    const auto scene = FpgaSceneCompiler().compile(source, error);
    if (!scene.isValid()) return {};
    return FpgaPacketCompiler().compile(scene, error);
}
}

// ProtocolTests - проверки протокола, моделей, интерфейса, текста и UART-сеанса.
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
    void editorProjectRoundTrip() {
        auto *p = ProjectManager::instance();
        QTemporaryDir temp;
        auto settings = ApplicationSettings::create();
        const QStringList keys{"gridStep", "gridColor", "snapCanvasGuideColor", "snapGridGuideColor", "snapObjectGuideColor"};
        QMap<QString, QVariant> previous;
        for (const auto &key : keys) previous.insert(key, settings.value(key));
        settings.setValue("gridStep", 27); settings.setValue("gridColor", "#53667788");
        settings.setValue("snapCanvasGuideColor", "#aaff5c7a"); settings.setValue("snapGridGuideColor", "#bb56d3ff"); settings.setValue("snapObjectGuideColor", "#ccffca58");
        p->createNewProject("Кадр 汉字", 1024, 768, QColor("#80abcdef"));
        for (const auto &type : {"rectangle_a", "dashline", "RibonScale", "aviahorizont", "staticgroup", "rotationobject", "layer3d", "text"}) QVERIFY(p->addObject(type) >= 0);
        RectangleObject *rectangle = nullptr;
        for (const auto &object : p->getObjects()) {
            if (auto *v = dynamic_cast<RectangleObject *>(object.data())) {
                rectangle = v; v->x = 35.25; v->y = 48.125; v->width = 126.12345678901234; v->height = 73.75;
                v->fillColor = QColor("#80f35c7a"); v->strokeColor = QColor("#8066ccff"); v->strokeWidth = 3.625; v->alpha = 128; v->explicitAlpha = true;
            } else if (auto *v = dynamic_cast<DashedLineObject *>(object.data())) { v->x0 = 25.125; v->y1 = 75.375; v->dashPhase = 3; }
            else if (auto *v = dynamic_cast<RibbonScaleObject *>(object.data())) { v->yStart = 16.125; v->left = 35.875; }
            else if (auto *v = dynamic_cast<AviaHorizonObject *>(object.data())) { v->lineWidth = 2.875; v->sinVal = 1234.25; v->cosVal = 65500.5; }
            else if (auto *v = dynamic_cast<RotationObject *>(object.data())) { v->left = 40.125; v->top = 50.25; v->xRot = 100.375; v->sinVal = 0.125; }
            else if (auto *v = dynamic_cast<TextObject *>(object.data())) {
                QVERIFY(v->setObjectProperty("Текст", "AV\nЯ α")); v->exportAlphabetGroups = {"digits", "latin_upper", "cyrillic_upper"};
                v->extraCharacters = "α°+-/";
            }
        }
        QVERIFY(rectangle);
        p->threeDViewObject()->setLeftAnchored(true);
        QVERIFY(p->setAircraftState(123.25, 345.5, 1000.75, 31.15, 1009));
        QString init = p->threeDViewObject()->serializeInit(); BitParser::inject(init, 84, 32, 0x89ABCDEF);
        p->threeDViewObject()->parse(init, FpgaSchemaRegistry::instance()->buildSchema("layer3d"));
        const int imagesLayer = p->addEditorLayer(); QVERIFY(p->renameEditorLayer(imagesLayer, "Изображения <&> 汉字"));
        QVERIFY(p->setEditorLayerColor(imagesLayer, QColor("#8bdff2")));
        QImage source(12, 9, QImage::Format_ARGB32); source.fill(Qt::red); source.setPixelColor(0, 0, Qt::green);
        const auto raster = temp.filePath("source.png"); QVERIFY(source.save(raster));
        QVERIFY(p->importImage(raster) >= 0);
        auto *image = dynamic_cast<ImageObject *>(p->getObjectAt(0).data()); QVERIFY(image);
        image->x = 100.125; image->width = 37.625;
        auto colors = image->colorLayers(); QVERIFY(!colors.isEmpty()); colors[0].autoMaskColor = false; colors[0].maskColor = QColor("#00c8f0"); image->setColorLayers(colors);
        QVERIFY(p->copyObjects({0})); QCOMPARE(p->pasteObjects().size(), 1);
        const QByteArray svg("<svg xmlns='http://www.w3.org/2000/svg' width='12' height='9'><rect width='12' height='9' fill='#ffca58'/></svg>");
        const auto vector = temp.filePath("source.svg"); QFile svgFile(vector); QVERIFY(svgFile.open(QIODevice::WriteOnly)); svgFile.write(svg); svgFile.close();
        QVERIFY(p->importImage(vector, true) >= 0);
        dynamic_cast<ImageObject *>(p->getObjectAt(0).data())->rotationDegrees = 37.125;
        const int emptyLayer = p->addEditorLayer(); QVERIFY(p->renameEditorLayer(emptyLayer, "Пустой"));
        const int group = p->groupObjects({0, 1, 3}); QVERIFY(group >= 0); QVERIFY(p->renameGroup(group, "Приборы <&> 🚀"));
        for (int i = 0; i < p->getObjectCount(); ++i) {
            auto object = p->getObjectAt(i);
            object->setCustomName(QString("Объект %1 <&> 🚀 汉字").arg(i));
            object->setViewVisible(i % 2 == 0); object->setSelectionLocked(i % 3 == 0);
            object->setResizeLocked(i % 3 == 1); object->setImportedHardwareObject(i % 4 == 0);
            object->setEditRestrictionHint(QString("Ограничение %1").arg(i));
        }
        QVERIFY(p->setObjectExportEnabled(0, false)); // Исходный SVG обязан остаться в проекте.
        p->setActiveLayer(imagesLayer); p->setShowAllLayers(true);
        p->setShowGrid(true); p->setSnapToCanvas(false); p->setSnapToGrid(true); p->setSnapToObjects(true);
        p->setEditorViewState({2.375, QPointF(-47.5, 28.125), true, true});
        const auto original = p->getObjects(); const auto hardware = p->buildCompiledFpgaXml().toByteArray();
        QString error; const auto bundle = FpgaProjectPacketBuilder::buildCurrentProject(&error); QVERIFY2(bundle.document.isValid(), qPrintable(error));
        const auto bytes = FpgaUartTransport::sceneBytes(bundle);
        auto render = [p]() {
            QImage frame(1024, 768, QImage::Format_ARGB32); frame.fill(p->getBackgroundColor()); QPainter painter(&frame);
            for (auto it = p->getObjects().crbegin(); it != p->getObjects().crend(); ++it) if ((*it)->isViewVisible()) (*it)->draw(painter);
            return frame;
        };
        const auto preview = render();
        const auto file = temp.filePath("project.xml"); QVERIFY2(p->saveToFile(file), qPrintable(p->lastErrorMessage()));
        QFile saved(file); QVERIFY(saved.open(QIODevice::ReadOnly)); const auto savedBytes = saved.readAll(); saved.close();
        QVERIFY(savedBytes.startsWith("<?xml version='1.0' encoding='windows-1251'?>"));
        QDomDocument document; QVERIFY(document.setContent(savedBytes));
        const auto editor = document.documentElement().firstChildElement("editor"); QVERIFY(!editor.isNull()); QCOMPARE(editor.attribute("version"), QString("1"));
        QDomDocument savedHardware;
        savedHardware.appendChild(savedHardware.importNode(document.documentElement(), true));
        savedHardware.documentElement().removeChild(savedHardware.documentElement().firstChildElement("editor"));
        QCOMPARE(savedHardware.toByteArray(), hardware); // Единственное дополнение к прежнему аппаратному XML — <editor>.
        const auto resources = editor.firstChildElement("resources"); QSet<QString> hashes; int binaries = 0;
        for (auto n = resources.firstChildElement("binary"); !n.isNull(); n = n.nextSiblingElement("binary")) { ++binaries; QVERIFY(!hashes.contains(n.attribute("sha256"))); hashes.insert(n.attribute("sha256")); }
        QVERIFY(binaries > 0);
        QFile::remove(raster); QFile::remove(vector);
        settings.setValue("gridStep", 3); p->createNewProject("Другой", 320, 240, Qt::black);
        MainWindow window; QTest::qWait(5);
        SettingsWindow projectSettings; projectSettings.show(); QTest::qWait(5);
        QCOMPARE(projectSettings.findChild<QSpinBox *>("GridStepSpin")->value(), 3); projectSettings.hide();
        QVERIFY2(p->loadFromFile(file), qPrintable(p->lastErrorMessage()));
        projectSettings.show(); QTest::qWait(5);
        QCOMPARE(projectSettings.findChild<QSpinBox *>("GridStepSpin")->value(), 27); projectSettings.hide();
        QCOMPARE(p->getObjects().size(), original.size()); QCOMPARE(p->editorLayers().size(), 3); QCOMPARE(p->layerObjectCount(emptyLayer), 0);
        QCOMPARE(p->activeLayerId(), imagesLayer); QVERIFY(p->showAllLayers()); QCOMPARE(p->groupMembers(group), (QList<int>{0, 1, 3}));
        QCOMPARE(p->groupName(group), QString("Приборы <&> 🚀"));
        for (int i = 0; i < original.size(); ++i) {
            const auto restored = p->getObjectAt(i); const auto before = original[i];
            QCOMPARE(restored->getTypeName(), before->getTypeName()); QCOMPARE(restored->getProperties(), before->getProperties());
            QCOMPARE(restored->customName(), before->customName()); QCOMPARE(restored->editorLayerId(), before->editorLayerId());
            QCOMPARE(restored->isViewVisible(), before->isViewVisible()); QCOMPARE(restored->isSelectionLocked(), before->isSelectionLocked());
            QCOMPARE(restored->isExportEnabled(), before->isExportEnabled()); QCOMPARE(restored->isResizeLocked(), before->isResizeLocked());
            QCOMPARE(restored->isImportedHardwareObject(), before->isImportedHardwareObject()); QCOMPARE(restored->editRestrictionHint(), before->editRestrictionHint());
            if (auto *v = dynamic_cast<ImageObject *>(restored.data())) QCOMPARE(v->sourcePayload(), dynamic_cast<ImageObject *>(before.data())->sourcePayload());
            if (auto *v = dynamic_cast<RectangleObject *>(restored.data())) { QCOMPARE(v->x, 35.25); QCOMPARE(v->width, 126.12345678901234); QCOMPARE(v->strokeWidth, 3.625); QCOMPARE(v->alpha, 128); }
        }
        QCOMPARE(p->getBackgroundColor(), QColor("#80abcdef")); QCOMPARE(p->gridStep(), 27); QCOMPARE(p->gridColor(), QColor("#53667788"));
        QCOMPARE(p->snapCanvasGuideColor(), QColor("#aaff5c7a")); QCOMPARE(p->snapGridGuideColor(), QColor("#bb56d3ff")); QCOMPARE(p->snapObjectGuideColor(), QColor("#ccffca58"));
        QVERIFY(p->showGrid()); QVERIFY(!p->snapToCanvas()); QVERIFY(p->snapToGrid()); QVERIFY(p->snapToObjects());
        QCOMPARE(p->editorViewState().offset, QPointF(-47.5, 28.125)); QCOMPARE(window.findChild<ViewportPanel *>()->getScale(), 2.375);
        QVERIFY(!window.findChild<QAction *>("ExportFpgaXmlAction"));
        QVERIFY(window.findChild<QAction *>("ProportionalResizeAction")->isChecked()); QVERIFY(window.findChild<QAction *>("SteppedRotationAction")->isChecked());
        QCOMPARE(p->threeDViewObject()->serializeInit(), init); QVERIFY(p->threeDViewObject()->isLeftAnchored());
        QCOMPARE(render(), preview); QCOMPARE(p->buildCompiledFpgaXml().toByteArray(), hardware);
        QCOMPARE(FpgaUartTransport::sceneBytes(FpgaProjectPacketBuilder::buildCurrentProject(&error)), bytes);
        QVERIFY2(p->saveToFile(file), qPrintable(p->lastErrorMessage()));
        QVERIFY(saved.open(QIODevice::ReadOnly)); QCOMPARE(saved.readAll(), savedBytes); saved.close();
        QVERIFY(p->renameGroup(group, "После открытия")); QVERIFY(p->undo()); QCOMPARE(p->groupName(group), QString("Приборы <&> 🚀"));
        for (const auto &key : keys) { if (previous.value(key).isValid()) settings.setValue(key, previous.value(key)); else settings.remove(key); }
        p->createNewProject("Done", 640, 480, Qt::black);
    }
    void editorProjectFontAtlasAndWhitespace() {
        auto *p = ProjectManager::instance(); QTemporaryDir temp;
        QString error;
        const auto source = FpgaCompiledDocument::fromDomDocument(textFixture(), &error);
        QVERIFY2(source.isValid(), qPrintable(error));
        p->createNewProject("Font atlas", 80, 50, BitParser::parseColor(0x203040)); p->addObject("text");
        auto *text = dynamic_cast<TextObject *>(p->getObjectAt(0).data()); QVERIFY(text);
        text->text = "AVA A"; text->setFontAtlas(source.fonts.value(0), true); text->setResizeLocked(true);
        const auto font = text->fontAtlas(); const auto props = text->getProperties();
        const auto project = temp.filePath("native.xml"); QVERIFY2(p->saveToFile(project), qPrintable(p->lastErrorMessage()));
        QFile native(project); QVERIFY(native.open(QIODevice::ReadOnly)); const auto originalNative = native.readAll(); native.close();
        QVERIFY2(p->loadFromFile(project), qPrintable(p->lastErrorMessage()));
        text = dynamic_cast<TextObject *>(p->getObjectAt(0).data()); QVERIFY(text && text->hasFontAtlas());
        QCOMPARE(text->getProperties(), props); QCOMPARE(text->fontAtlas().maskData, font.maskData); QCOMPARE(text->fontAtlas().kerningPairs, font.kerningPairs);
        QCOMPARE(text->fontAtlas().glyphOrder, font.glyphOrder); QVERIFY(text->restrictedAtlasEditing); QVERIFY(text->isResizeLocked());
        QVERIFY(p->saveToFile(project)); QVERIFY(native.open(QIODevice::ReadOnly)); QCOMPARE(native.readAll(), originalNative); native.close();
        p->createNewProject("Whitespace", 640, 480, Qt::black); p->addObject("text");
        text = dynamic_cast<TextObject *>(p->getObjectAt(0).data());
        const QString whitespace = "  \r\n\t "; QVERIFY(text->setObjectProperty("Текст", whitespace));
        p->setObjectExportEnabled(0, false);
        QVERIFY2(p->saveToFile(project), qPrintable(p->lastErrorMessage())); QVERIFY2(p->loadFromFile(project), qPrintable(p->lastErrorMessage()));
        QCOMPARE(dynamic_cast<TextObject *>(p->getObjectAt(0).data())->text, whitespace);
        QCOMPARE(p->getObjectCount(), 1); QVERIFY(!p->getObjectAt(0)->isExportEnabled());
    }
    void editorProjectRejectsCorruptMetadataAtomically() {
        auto *p = ProjectManager::instance(); QTemporaryDir temp;
        p->createNewProject("Keep", 640, 480, Qt::black); p->addObject("rectangle"); p->addObject("staticgroup");
        p->groupObjects({0, 1}); p->addEditorLayer();
        const auto file = temp.filePath("project.xml"); QVERIFY2(p->saveToFile(file), qPrintable(p->lastErrorMessage()));
        QFile saved(file); QVERIFY(saved.open(QIODevice::ReadOnly)); QDomDocument source; QVERIFY(source.setContent(saved.readAll())); saved.close();
        QVERIFY(p->renameObject(0, "Keep current"));
        const auto objects = p->getObjects(); const auto path = p->getFilePath();
        using Mutator = std::function<void(QDomElement)>;
        const QList<Mutator> changes = {
            [](auto e) { e.setAttribute("version", "99"); },
            [](auto e) { e.firstChildElement("layers").setAttribute("active-id", "99999"); },
            [](auto e) { e.firstChildElement("workspace").firstChildElement("view").setAttribute("scale", "nan"); },
            [](auto e) { e.firstChildElement("scene").firstChildElement("object").setAttribute("layer-id", "99999"); },
            [](auto e) { e.firstChildElement("scene").firstChildElement("object").setAttribute("type", "unknown"); },
            [](auto e) { auto first = e.firstChildElement("scene").firstChildElement("object"); first.nextSiblingElement("object").setAttribute("id", first.attribute("id")); },
            [](auto e) { e.firstChildElement("groups").firstChildElement("group").firstChildElement("member").setAttribute("ref", "missing"); },
            [](auto e) { e.firstChildElement("resources").firstChildElement("binary").setAttribute("sha256", "bad"); },
            [](auto e) { e.removeChild(e.firstChildElement("workspace")); },
            [](auto e) { e.parentNode().appendChild(e.cloneNode(true)); },
            [](auto e) { e.parentNode().removeChild(e); }
        };
        for (int i = 0; i < changes.size(); ++i) {
            auto doc = source.cloneNode(true).toDocument(); changes[i](doc.documentElement().firstChildElement("editor"));
            const auto bad = temp.filePath(QString("bad-%1.xml").arg(i)); QFile out(bad); QVERIFY(out.open(QIODevice::WriteOnly)); out.write(doc.toByteArray()); out.close();
            QVERIFY2(!p->loadFromFile(bad), qPrintable(QString::number(i))); QVERIFY(!p->lastErrorMessage().isEmpty());
            QCOMPARE(p->getObjects(), objects); QCOMPARE(p->getFilePath(), path); QCOMPARE(p->getObjectAt(0)->customName(), QString("Keep current")); QVERIFY(p->canUndo());
        }
    }
    void editorLayerLifecycle() {
        auto *p = ProjectManager::instance();
        p->createNewProject("Layers", 640, 480, Qt::black);
        QCOMPARE(p->editorLayers().size(), 1);
        QVERIFY(!p->removeEditorLayer(1));
        p->addObject("rectangle");
        auto bottom = p->getObjectAt(0);
        const int layer = p->addEditorLayer();
        QVERIFY(layer != 1); QCOMPARE(p->activeLayerId(), layer);
        p->addObject("dashline");
        auto top = p->getObjectAt(0);
        QCOMPARE(top->editorLayerId(), layer); QCOMPARE(bottom->editorLayerId(), 1);
        const auto xml = p->buildCompiledFpgaXml().toByteArray();
        QVERIFY(p->renameEditorLayer(layer, "Индикация"));
        QVERIFY(!p->renameEditorLayer(1, "  Индикация  "));
        QVERIFY(!p->renameEditorLayer(1, "   "));
        QVERIFY(p->setEditorLayerColor(layer, QColor("#8bdff2")));
        QVERIFY(p->undo()); QCOMPARE(p->editorLayerColor(layer), QColor("#ff5c7a"));
        QVERIFY(p->undo()); QCOMPARE(p->editorLayerName(layer), QString("Слой 2"));
        QVERIFY(p->redo()); QCOMPARE(p->editorLayerName(layer), QString("Индикация"));
        QVERIFY(p->redo()); QCOMPARE(p->editorLayerColor(layer), QColor("#8bdff2"));
        top = p->getObjectAt(0); bottom = p->getObjectAt(1);
        p->setActiveLayer(1);
        QVERIFY(p->isObjectVisibleInEditor(bottom.data()));
        QVERIFY(!p->isObjectVisibleInEditor(top.data()));
        p->setShowAllLayers(true);
        QVERIFY(p->isObjectVisibleInEditor(top.data()));
        QCOMPARE(p->buildCompiledFpgaXml().toByteArray(), xml);
        QVERIFY(p->setObjectsLayer({bottom}, layer));
        QCOMPARE(p->layerObjectCount(layer), 2);
        QCOMPARE(p->buildCompiledFpgaXml().toByteArray(), xml);
        QVERIFY(p->undo()); QCOMPARE(p->layerObjectCount(1), 1);
        QVERIFY(p->redo()); QCOMPARE(p->layerObjectCount(layer), 2);
        const int duplicate = p->duplicateEditorLayer(layer);
        QVERIFY(duplicate > layer); QCOMPARE(p->layerObjectCount(duplicate), 2);
        QCOMPARE(p->getObjectCount(), 4);
        QCOMPARE(p->getObjectAt(0)->getTypeName(), p->getObjectAt(2)->getTypeName());
        QCOMPARE(p->getObjectAt(0)->getBoundingRect(), p->getObjectAt(2)->getBoundingRect());
        const auto order = p->getObjects();
        QVERIFY(p->mergeEditorLayer(duplicate, 1));
        QCOMPARE(p->getObjects(), order); QCOMPARE(p->layerObjectCount(1), 2);
        QVERIFY(!p->hasEditorLayer(duplicate));
        QVERIFY(p->undo()); QVERIFY(p->hasEditorLayer(duplicate));
        QVERIFY(p->removeEditorLayer(duplicate)); QCOMPARE(p->getObjectCount(), 2);
        QVERIFY(p->undo()); QCOMPARE(p->getObjectCount(), 4);
        QVERIFY(p->redo()); QCOMPARE(p->getObjectCount(), 2);
        QTemporaryDir temp;
        const auto file = temp.filePath("layers.xml");
        const auto layers = p->editorLayers(); const int activeLayer = p->activeLayerId();
        QList<int> objectLayers; for (const auto &object : p->getObjects()) objectLayers.append(object->editorLayerId());
        QVERIFY(p->saveToFile(file)); QVERIFY(p->loadFromFile(file));
        QCOMPARE(p->editorLayers().size(), layers.size()); QCOMPARE(p->activeLayerId(), activeLayer);
        for (int i = 0; i < p->getObjectCount(); ++i) QCOMPARE(p->getObjectAt(i)->editorLayerId(), objectLayers[i]);
    }
    void mergeMultipleEditorLayers() {
        auto *p = ProjectManager::instance();
        p->createNewProject("Multi layer merge", 640, 480, Qt::black);
        p->addObject("rectangle");
        const int second = p->addEditorLayer(); p->addObject("rectangle");
        const int target = p->addEditorLayer(); p->addObject("dashline");
        const int untouched = p->addEditorLayer(); p->addObject("rectangle");
        QVERIFY(p->renameEditorLayer(target, "Индикация"));
        QVERIFY(p->setEditorLayerColor(target, QColor("#8bdff2")));
        p->groupObjects({1, 2});
        const auto objects = p->getObjects();
        const auto groups = p->objectGroups();
        const auto xml = p->buildCompiledFpgaXml().toByteArray();
        QSignalSpy changed(p, &ProjectManager::projectChanged);
        QVERIFY(!p->mergeEditorLayers({1, second, -1}, target));
        QVERIFY(!p->mergeEditorLayers({target, target}, target));
        QVERIFY(!p->mergeEditorLayers({1, second}, target));
        QVERIFY(changed.isEmpty()); QCOMPARE(p->editorLayers().size(), 4);
        QVERIFY(p->mergeEditorLayers({target, 1, second, second}, target));
        QCOMPARE(changed.size(), 1); QCOMPARE(p->editorLayers().size(), 2);
        QCOMPARE(p->activeLayerId(), target); QCOMPARE(p->layerObjectCount(target), 3);
        QCOMPARE(p->layerObjectCount(untouched), 1); QCOMPARE(p->getObjects(), objects);
        QCOMPARE(p->editorLayerName(target), QString("Индикация"));
        QCOMPARE(p->editorLayerColor(target), QColor("#8bdff2"));
        QCOMPARE(p->objectGroups().first().members, groups.first().members);
        QCOMPARE(p->buildCompiledFpgaXml().toByteArray(), xml);
        QVERIFY(p->undo()); QCOMPARE(p->editorLayers().size(), 4);
        QCOMPARE(p->layerObjectCount(1), 1); QCOMPARE(p->layerObjectCount(second), 1); QCOMPARE(p->layerObjectCount(target), 1);
        QVERIFY(p->redo()); QCOMPARE(p->editorLayers().size(), 2); QCOMPARE(p->layerObjectCount(target), 3);
    }
    void layersMultiSelectionAndMerge() {
        auto *p = ProjectManager::instance();
        p->createNewProject("Layer selection", 640, 480, Qt::black);
        QList<int> ids{1}; p->addObject("rectangle");
        for (int i = 0; i < 4; ++i) { ids.append(p->addEditorLayer()); p->addObject("rectangle"); }
        LayersPanel panel; panel.resize(280, 300); panel.show(); QTest::qWait(5);
        auto nameFor = [&panel](int id) -> QLineEdit * {
            for (auto *name : panel.findChildren<QLineEdit *>("LayerNameEdit"))
                if (name->property("layerId").toInt() == id) return name;
            return nullptr;
        };
        auto selected = [&panel]() {
            QSet<int> ids;
            for (auto *row : panel.findChildren<QWidget *>("LayerRow"))
                if (row->property("selected").toBool()) ids.insert(row->property("layerId").toInt());
            return ids;
        };
        auto click = [&](int index, Qt::KeyboardModifiers modifiers = {}) {
            auto *name = nameFor(ids[index]); QVERIFY(name);
            QTest::mouseClick(name, Qt::LeftButton, modifiers);
        };
        auto *scroll = panel.findChild<QScrollArea *>("LayersScrollArea"); QVERIFY(scroll);
        QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
        for (auto *button : scroll->findChildren<QToolButton *>()) {
            const int x = button->mapTo(scroll->viewport(), QPoint()).x();
            QVERIFY(x >= 0); QVERIFY(x + button->width() <= scroll->viewport()->width());
            QCOMPARE(button->size(), QSize(32, 32));
            if (button->objectName() != "LayerColorButton") QCOMPARE(button->iconSize(), QSize(18, 18));
        }
        click(0); QCOMPARE(selected(), QSet<int>{ids[0]});
        auto *merge = nameFor(ids[0])->parentWidget()->findChild<QToolButton *>("MergeLayerButton");
        QVERIFY(merge); QVERIFY(!merge->isEnabled());
        click(2, Qt::ControlModifier); QCOMPARE(selected(), (QSet<int>{ids[0], ids[2]}));
        QCOMPARE(p->activeLayerId(), ids[2]); QVERIFY(merge->isEnabled());
        QVERIFY(!nameFor(ids[1])->parentWidget()->findChild<QToolButton *>("MergeLayerButton")->isEnabled());
        click(1, Qt::MetaModifier); QCOMPARE(selected().size(), 3);
        click(1, Qt::ControlModifier); QCOMPARE(selected(), (QSet<int>{ids[0], ids[2]}));
        QVERIFY(selected().contains(p->activeLayerId()));
        click(0); click(3, Qt::ShiftModifier);
        QCOMPARE(selected(), (QSet<int>{ids[0], ids[1], ids[2], ids[3]}));
        QCOMPARE(p->activeLayerId(), ids[3]);
        p->setShowAllLayers(true); QCOMPARE(selected().size(), 4);
        panel.refreshLayers(); QCOMPARE(selected().size(), 4);
        scroll->verticalScrollBar()->setValue(0);
        panel.grab().save(QCoreApplication::applicationDirPath() + "/layers-multiselection.png");
        merge = nameFor(ids[3])->parentWidget()->findChild<QToolButton *>("MergeLayerButton"); QVERIFY(merge && merge->isEnabled());
        QSignalSpy changed(p, &ProjectManager::projectChanged);
        merge->click();
        QVERIFY(!QApplication::activeModalWidget());
        QCOMPARE(changed.size(), 1); QCOMPARE(p->editorLayers().size(), 2);
        QCOMPARE(p->activeLayerId(), ids[3]); QCOMPARE(p->layerObjectCount(ids[3]), 4);
        QTest::qWait(5); QCOMPARE(selected(), QSet<int>{ids[3]});
        QVERIFY(p->undo()); QCOMPARE(p->editorLayers().size(), 5);
        QVERIFY(p->redo()); QCOMPARE(p->editorLayers().size(), 2);
        QTest::qWait(5);
        p->setActiveLayer(ids[4]); QCOMPARE(selected(), QSet<int>{ids[4]});
        // A normal click selects; a double click or F2 edits the name.
        auto *name = nameFor(ids[4]); QVERIFY(name && name->isReadOnly());
        QTest::mouseDClick(name, Qt::LeftButton); QVERIFY(!name->isReadOnly());
        name->setText("Переименованный"); QTest::keyClick(name, Qt::Key_Return);
        QCOMPARE(p->editorLayerName(ids[4]), QString("Переименованный"));
        QTest::qWait(5); name = nameFor(ids[4]); QVERIFY(name && name->isReadOnly());
        QTest::keyClick(name, Qt::Key_F2); name->setText("Отменить"); QTest::keyClick(name, Qt::Key_Escape);
        QCOMPARE(name->text(), QString("Переименованный")); QVERIFY(name->isReadOnly());
        p->createNewProject("Reset", 640, 480, Qt::black);
        QCOMPARE(selected(), QSet<int>{1});
    }
    void editorLayerGroupsAndClipboard() {
        auto *p = ProjectManager::instance();
        p->createNewProject("Groups", 640, 480, Qt::black);
        p->addObject("rectangle"); p->addObject("rectangle");
        const int group = p->groupObjects({0, 1}); QVERIFY(group >= 0);
        QVERIFY(p->copyObjects({0, 1}));
        const int layer = p->addEditorLayer();
        const auto pasted = p->pasteObjects(); QCOMPARE(pasted.size(), 2);
        for (int index : pasted) QCOMPARE(p->getObjectAt(index)->editorLayerId(), layer);
        const int duplicate = p->duplicateEditorLayer(layer); QVERIFY(duplicate > layer);
        QCOMPARE(p->objectGroups().size(), 3);
        QVERIFY(p->removeEditorLayer(layer)); QCOMPARE(p->objectGroups().size(), 2);
        for (const auto &g : p->objectGroups()) {
            QCOMPARE(g.members.size(), 2);
            for (int index : g.members) QVERIFY(p->getObjectAt(index));
            QCOMPARE(p->getObjectAt(g.members[0])->editorLayerId(), p->getObjectAt(g.members[1])->editorLayerId());
        }
        QVERIFY(p->undo()); QCOMPARE(p->objectGroups().size(), 3);
        p->createNewProject("3D", 1920, 1080, Qt::black); p->addObject("layer3d");
        QVERIFY(p->duplicateEditorLayer(1) < 0);
        QCOMPARE(p->editorLayers().size(), 1); QCOMPARE(p->getObjectCount(), 1);
    }
    void layerSelectionAndProperties() {
        auto *p = ProjectManager::instance();
        p->createNewProject("Selection", 640, 480, Qt::black);
        p->addObject("rectangle"); auto first = p->getObjectAt(0);
        const int layer = p->addEditorLayer();
        p->addObject("rectangle"); auto second = p->getObjectAt(0);
        ViewportPanel viewport;
        viewport.setSelectedIndexes({0, 1}); QCOMPARE(viewport.getSelectedIndexes(), QList<int>{0});
        QSignalSpy changed(&viewport, &ViewportPanel::selectionChanged);
        p->setShowAllLayers(true);
        viewport.setSelectedIndexes({0, 1}); QCOMPARE(viewport.getSelectedIndexes().size(), 2);
        ObjectPropertiesPanel properties; properties.resize(360, 540); properties.show();
        properties.showSelectedObjects({0, 1});
        auto *table = properties.findChild<QTableWidget *>("PropertiesTable"); QVERIFY(table);
        auto *combo = properties.findChild<QComboBox *>("ObjectLayerCombo"); QVERIFY(combo);
        QCOMPARE(combo->count(), 2); QCOMPARE(combo->currentIndex(), -1);
        auto rowFor = [table](const QString &name) { for (int r = 0; r < table->rowCount(); ++r) if (table->item(r, 0)->text() == name) return r; return -1; };
        QVERIFY(rowFor("Ширина") >= 0); QVERIFY(rowFor("Заливка") >= 0);
        QVERIFY(p->setObjectProperty(first.data(), "Ширина", "75"));
        properties.showSelectedObjects({0, 1});
        QCOMPARE(table->item(rowFor("Ширина"), 1)->text(), QString("Разные значения"));
        table->item(rowFor("Ширина"), 1)->setText("90");
        QCOMPARE(first->getBoundingRect().width(), 90.); QCOMPARE(second->getBoundingRect().width(), 90.);
        QVERIFY(p->undo());
        QCOMPARE(p->getObjectAt(0)->getBoundingRect().width(), 120.);
        QCOMPARE(p->getObjectAt(1)->getBoundingRect().width(), 75.);
        first = p->getObjectAt(1); second = p->getObjectAt(0);
        QVERIFY(!p->setObjectsProperty({first, second}, "Ширина", "ошибка"));
        QCOMPARE(first->getBoundingRect().width(), 75.); QCOMPARE(second->getBoundingRect().width(), 120.);
        QVERIFY(p->setObjectsProperty({first, second}, "Заливка", "#ff5c7a"));
        QCOMPARE(dynamic_cast<RectangleObject *>(first.data())->fillColor.name(), QString("#ff5c7a"));
        properties.showSelectedObjects({0, 1});
        combo = properties.findChild<QComboBox *>("ObjectLayerCombo");
        combo->setCurrentIndex(combo->findData(layer)); combo->activated(combo->currentIndex());
        QCOMPARE(first->editorLayerId(), layer); QCOMPARE(second->editorLayerId(), layer);
        p->setShowAllLayers(false); p->setActiveLayer(1);
        QVERIFY(viewport.getSelectedIndexes().isEmpty()); QVERIFY(!changed.isEmpty());
        // Different object types expose the intersection of their property names.
        p->addObject("dashline"); properties.showSelectedObjects({0, 1});
        QVERIFY(rowFor("Слой") >= 0); QVERIFY(rowFor("Заливка") < 0);
        // One restricted object rejects the entire batch without changing the first.
        p->createNewProject("Atomic", 1920, 1080, Qt::black);
        p->addObject("rectangle"); p->addObject("layer3d");
        auto rectangle = p->getObjectAt(1); const auto bounds = rectangle->getBoundingRect();
        QVERIFY(!p->setObjectsProperty({rectangle, p->getObjectAt(0)}, "X", "25"));
        QCOMPARE(rectangle->getBoundingRect(), bounds);
    }
    void layerRenderingAndBatchTransparency() {
        auto *p = ProjectManager::instance();
        p->createNewProject("Layer colors", 640, 480, Qt::black);
        p->addObject("rectangle"); auto first = p->getObjectAt(0);
        const int layer = p->addEditorLayer();
        p->addObject("rectangle"); auto second = p->getObjectAt(0);
        auto *a = dynamic_cast<RectangleObject *>(first.data());
        auto *b = dynamic_cast<RectangleObject *>(second.data()); QVERIFY(a && b);
        a->x = 60.5; a->y = 60.5; a->width = 80; a->height = 60; a->strokeWidth = 0; a->fillColor = Qt::white;
        b->x = 260.5; b->y = 60.5; b->width = 80; b->height = 60; b->strokeWidth = 0; b->fillColor = Qt::white;
        ViewportPanel viewport; viewport.resize(640, 480); viewport.show(); QTest::qWait(5);
        // A hidden layer neither draws nor receives clicks.
        QTest::mouseClick(&viewport, Qt::LeftButton, Qt::NoModifier, QPoint(110, 95));
        QVERIFY(viewport.getSelectedIndexes().isEmpty());
        auto image = viewport.grab().toImage(); QCOMPARE(image.pixelColor(110, 95), QColor(Qt::black));
        p->setShowAllLayers(true); viewport.setSelectedIndexes({0, 1});
        image = viewport.grab().toImage();
        auto colorPixels = [&image](const QRect &region, const QColor &target) {
            int count = 0;
            for (int y = region.top(); y <= region.bottom(); ++y) for (int x = region.left(); x <= region.right(); ++x) {
                const QColor c = image.pixelColor(x, y);
                if (qAbs(c.red() - target.red()) < 25 && qAbs(c.green() - target.green()) < 25 && qAbs(c.blue() - target.blue()) < 25) ++count;
            }
            return count;
        };
        QVERIFY(colorPixels(QRect(70, 66, 85, 65), p->editorLayerColor(1)) > 20);
        QVERIFY(colorPixels(QRect(260, 66, 85, 65), p->editorLayerColor(layer)) > 20);
        image.save(QCoreApplication::applicationDirPath() + "/layer-selection-colors.png");
        QVERIFY(p->setObjectProperty(first.data(), "Прозрачность", "50"));
        ObjectPropertiesPanel properties; properties.showSelectedObjects({0, 1});
        auto *spin = properties.findChild<QSpinBox *>("TransparencySpin");
        auto *slider = properties.findChild<QSlider *>("TransparencySlider"); QVERIFY(spin && slider);
        QCOMPARE(spin->value(), -1); QCOMPARE(spin->text(), QString("Разные"));
        slider->sliderPressed(); slider->setValue(0); slider->sliderReleased();
        QCOMPARE(a->alpha, 255); QCOMPARE(b->alpha, 255); QCOMPARE(spin->minimum(), 0);
        QCOMPARE(spin->text(), QString("0 %")); QVERIFY(spin->specialValueText().isEmpty());
        properties.showObjectProperties(0);
        QCOMPARE(properties.findChild<QSpinBox *>("TransparencySpin")->text(), QString("0 %"));
        QVERIFY(p->undo());
        QCOMPARE(dynamic_cast<RectangleObject *>(p->getObjectAt(1).data())->alpha, 128);
        QCOMPARE(dynamic_cast<RectangleObject *>(p->getObjectAt(0).data())->alpha, 255);
        const auto before = p->getObjectAt(1)->getBoundingRect();
        QVERIFY(!p->setObjectsProperty(p->getObjects(), "Ширина", "nan"));
        QVERIFY(!p->setObjectsProperty(p->getObjects(), "Ширина", "-10"));
        QCOMPARE(p->getObjectAt(1)->getBoundingRect(), before);
    }
    void comboPopupGeometry_data() {
        QTest::addColumn<QString>("name");
        QTest::addColumn<bool>("dialog");
        QTest::newRow("workspace") << QString("WorkspaceLayerCombo") << false;
        QTest::newRow("object-layer") << QString("ObjectLayerCombo") << false;
        QTest::newRow("property") << QString("PropertyValueCombo") << false;
        QTest::newRow("dialog") << QString("DialogCombo") << true;
    }
    void comboPopupGeometry() {
        QFETCH(QString, name);
        QFETCH(bool, dialog);
        std::unique_ptr<QWidget> host(dialog ? static_cast<QWidget *>(new QDialog) : new QWidget);
        host->resize(360, 320);
        auto *combo = new QComboBox(host.get());
        combo->setObjectName(name); combo->setGeometry(20, 20, 240, 36);
        for (int i = 0; i < 6; ++i) combo->addItem(QString("Пункт %1").arg(i + 1));
        host->show(); QTest::qWait(5);
        combo->showPopup(); QTest::qWait(5);
        auto *view = combo->view(); QVERIFY(view->isVisible());
        for (int row = 0; row < combo->count(); ++row) {
            const QRect rect = view->visualRect(combo->model()->index(row, 0));
            QVERIFY2(rect.height() >= combo->fontMetrics().height(), qPrintable(QString("row=%1; height=%2").arg(row).arg(rect.height())));
            QVERIFY2(view->viewport()->rect().contains(rect), qPrintable(QString("%1 row %2: view=%3x%4; item=%5,%6 %7x%8; popup=%9x%10; style=%11")
                .arg(name).arg(row).arg(view->viewport()->width()).arg(view->viewport()->height())
                .arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height())
                .arg(view->window()->width()).arg(view->window()->height()).arg(qApp->style()->objectName())));
        }
        const auto selected = view->visualRect(combo->model()->index(0, 0));
        const auto preview = view->viewport()->grab().toImage();
        const QPoint sample(selected.right() - 6, selected.center().y());
        const qreal ratio = preview.devicePixelRatio();
        QCOMPARE(preview.pixelColor(qRound(sample.x() * ratio), qRound(sample.y() * ratio)), QColor("#00c8f0"));
        view->window()->grab().save(QCoreApplication::applicationDirPath() + "/popup-" + name + ".png");
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->visualRect(combo->model()->index(5, 0)).center());
        QCOMPARE(combo->currentIndex(), 5);
        for (int i = 6; i < 30; ++i) combo->addItem(QString("Пункт %1").arg(i + 1));
        combo->showPopup(); QTest::qWait(5);
        QVERIFY(view->isVisible()); QVERIFY(view->verticalScrollBar()->maximum() > 0);
        view->scrollTo(combo->model()->index(29, 0)); QTest::qWait(5);
        const QRect last = view->visualRect(combo->model()->index(29, 0));
        QVERIFY(view->viewport()->rect().contains(last));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, last.center());
        QCOMPARE(combo->currentIndex(), 29);
    }
    void layersWindowWidthMigration() {
        auto settings = ApplicationSettings::create();
        {
            MainWindow window; QTest::qWait(5);
            auto *dock = window.findChild<QDockWidget *>("LayersDock"); QVERIFY(dock);
            dock->setFloating(true); dock->show(); dock->resize(440, 350); QTest::qWait(5);
            settings.setValue("windowGeometry", window.saveGeometry());
            settings.setValue("windowState", window.saveState());
            settings.remove("layersPanelLayoutVersion");
        }
        {
            MainWindow window; QTest::qWait(5);
            auto *dock = window.findChild<QDockWidget *>("LayersDock"); QVERIFY(dock);
            QCOMPARE(dock->width(), 320);
            QCOMPARE(settings.value("layersPanelLayoutVersion").toInt(), 1);
            dock->show(); dock->resize(360, 350); QTest::qWait(5);
            settings.setValue("windowGeometry", window.saveGeometry());
            settings.setValue("windowState", window.saveState());
        }
        {
            MainWindow window; QTest::qWait(5);
            auto *dock = window.findChild<QDockWidget *>("LayersDock"); QVERIFY(dock);
            QCOMPARE(dock->width(), 360);
        }
        settings.remove("windowGeometry"); settings.remove("windowState");
    }
    void layersInterfaceLayout() {
        auto *p = ProjectManager::instance();
        p->createNewProject("Layers UI", 640, 480, Qt::black);
        p->addObject("rectangle"); p->addEditorLayer(); p->addObject("dashline");
        LayersPanel layers; layers.resize(320, 350); layers.show(); QTest::qWait(5);
        auto *list = layers.findChild<QScrollArea *>("LayersScrollArea"); QVERIFY(list);
        QCOMPARE(list->widget()->findChildren<QWidget *>("LayerRow", Qt::FindDirectChildrenOnly).size(), 2);
        auto *all = layers.findChild<QToolButton *>("ShowAllLayersButton"); QVERIFY(all);
        all->click(); QVERIFY(p->showAllLayers());
        auto edits = layers.findChildren<QLineEdit *>("LayerNameEdit"); QCOMPARE(edits.size(), 2);
        QTest::mouseClick(edits[0], Qt::LeftButton); QTest::qWait(1); QCOMPARE(p->activeLayerId(), 1);
        QTest::mouseDClick(edits[0], Qt::LeftButton); QVERIFY(!edits[0]->isReadOnly());
        edits[0]->setText("Основной"); edits[0]->editingFinished(); QTest::qWait(5);
        QCOMPARE(p->editorLayerName(1), QString("Основной"));
        for (auto *button : layers.findChildren<QToolButton *>()) {
            QVERIFY2(layers.rect().contains(QRect(button->mapTo(&layers, QPoint()), button->size())), qPrintable(button->objectName()));
            if (button != all) {
                QCOMPARE(button->width(), button->height());
                QVERIFY(button->text().isEmpty());
                QVERIFY(!button->icon().isNull());
                QVERIFY(!button->icon().pixmap(button->iconSize()).isNull());
            }
        }
        auto *add = layers.findChild<QToolButton *>("AddLayerButton"); QVERIFY(add);
        QVERIFY(add->text().isEmpty()); QVERIFY(!add->icon().isNull());
        layers.grab().save(QCoreApplication::applicationDirPath() + "/layers-panel.png");
        layers.resize(280, 350); QTest::qWait(5);
        for (auto *button : list->findChildren<QToolButton *>())
            QVERIFY2(list->viewport()->rect().contains(QRect(button->mapTo(list->viewport(), QPoint()), button->size())),
                qPrintable(QString("%1; view=%2x%3; button x=%4; row min=%5; row sizehint=%6")
                .arg(button->objectName()).arg(list->viewport()->width()).arg(list->viewport()->height())
                .arg(button->mapTo(list->viewport(), QPoint()).x()).arg(button->parentWidget()->minimumWidth()).arg(button->parentWidget()->minimumSizeHint().width())));
        EditorWorkspacePanel workspace; workspace.resize(800, 500); workspace.show(); QTest::qWait(5);
        auto *combo = workspace.findChild<QComboBox *>("WorkspaceLayerCombo"); QVERIFY(combo);
        QCOMPARE(combo->count(), 2); QVERIFY(!combo->isEditable());
        QCOMPARE(combo->width(), 180);
        QCOMPARE(combo->parentWidget(), workspace.viewport());
        QCOMPARE(combo->pos(), QPoint(8, 8));
        QCOMPARE(workspace.viewport()->geometry().top(), workspace.selectionToolStrip()->geometry().top());
        QCOMPARE(workspace.viewport()->geometry().top(), workspace.layout()->contentsMargins().top());
        QVERIFY(!workspace.findChild<QLabel *>("WorkspaceLayerLabel"));
        workspace.resize(1200, 500); QTest::qWait(5); QCOMPARE(combo->width(), 180);
        combo->setCurrentIndex(1); combo->activated(1); QCOMPARE(p->activeLayerId(), combo->currentData().toInt());
        QVERIFY(workspace.viewport()->rect().contains(combo->geometry()));
        QCOMPARE(workspace.selectionToolStrip()->height(), workspace.viewport()->height());
        ObjectPropertiesPanel properties; properties.resize(360, 520); properties.showSelectedObjects({0, 1}); properties.show(); QTest::qWait(5);
        properties.grab().save(QCoreApplication::applicationDirPath() + "/multi-properties.png");
        // The persistent layer field and ordinary dropdown editors share the same geometry.
        p->addObject("text"); properties.showObjectProperties(0); QTest::qWait(5);
        auto *table = properties.findChild<QTableWidget *>("PropertiesTable"); QVERIFY(table);
        auto *layerCombo = properties.findChild<QComboBox *>("ObjectLayerCombo"); QVERIFY(layerCombo);
        const QRect layerRect(layerCombo->mapTo(table->viewport(), QPoint()), layerCombo->size());
        const int layerRow = table->rowCount() - 1;
        QCOMPARE(table->item(layerRow, 0)->text(), QString("Слой"));
        QVERIFY(table->visualItemRect(table->item(layerRow, 1)).contains(layerRect));
        QVERIFY(!table->visualItemRect(table->item(layerRow - 1, 1)).intersects(layerRect));
        int fontRow = -1;
        for (int row = 0; row < table->rowCount(); ++row)
            if (table->item(row, 0)->text() == "Шрифт") fontRow = row;
        QVERIFY(fontRow >= 0);
        table->editItem(table->item(fontRow, 1)); QTest::qWait(5);
        auto *fontCombo = table->findChild<QComboBox *>("PropertyValueCombo"); QVERIFY(fontCombo);
        QCOMPARE(fontCombo->size(), layerCombo->size());
        const QRect fontRect(fontCombo->mapTo(table->viewport(), QPoint()), fontCombo->size());
        QVERIFY(table->visualItemRect(table->item(fontRow, 1)).contains(fontRect));
        properties.grab().save(QCoreApplication::applicationDirPath() + "/property-dropdowns.png");
        MainWindow window; QTest::qWait(5); window.showNormal(); window.resize(1400, 900); window.show();
        auto *dock = window.findChild<QDockWidget *>("LayersDock"); QVERIFY(dock);
        dock->setFloating(false); window.addDockWidget(Qt::LeftDockWidgetArea, dock); dock->show();
        QTest::qWait(10);
        p->createNewProject("Интерфейс со слоями", 640, 480, Qt::black);
        p->addObject("rectangle"); p->setObjectProperty(p->getObjectAt(0).data(), "X", "80");
        p->renameEditorLayer(1, "Основной");
        const int foreground = p->addEditorLayer(); p->renameEditorLayer(foreground, "Индикация");
        p->addObject("rectangle"); p->setObjectProperty(p->getObjectAt(0).data(), "X", "280");
        p->setShowAllLayers(true);
        auto *viewport = window.findChild<ViewportPanel *>(); QVERIFY(viewport);
        viewport->setSelectedIndexes({0, 1}); viewport->selectionChanged({0, 1});
        QTest::qWait(5);
        auto *layerScroll = dock->findChild<QScrollArea *>("LayersScrollArea"); QVERIFY(layerScroll);
        QTRY_COMPARE(layerScroll->widget()->findChildren<QWidget *>("LayerRow", Qt::FindDirectChildrenOnly).size(), 2);
        for (auto *row : layerScroll->widget()->findChildren<QWidget *>("LayerRow", Qt::FindDirectChildrenOnly)) {
            QVERIFY2(row->isVisible(), qPrintable(QString("hidden %1").arg(row->property("layerId").toInt())));
            QVERIFY(layerScroll->viewport()->rect().contains(QRect(row->mapTo(layerScroll->viewport(), QPoint()), row->size())));
        }
        for (auto *button : dock->findChildren<QToolButton *>())
            QVERIFY2(dock->rect().contains(QRect(button->mapTo(dock, QPoint()), button->size())), qPrintable(button->objectName()));
        window.grab().save(QCoreApplication::applicationDirPath() + "/layers-workspace.png");
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
        QCOMPARE(p->objectGroups().size(), 1);
        QCOMPARE(p->groupMembers(group), (QList<int>{0, 1, 2}));
        for (const auto &object : p->getObjects()) {
            QVERIFY(!object->isViewVisible());
            QVERIFY(object->isSelectionLocked());
        }
        QVERIFY(p->threeDViewObject()->isLeftAnchored());
        const auto reloaded = FpgaCompiledDocument::fromDomDocument(p->buildCompiledFpgaXml(), &error);
        QCOMPARE(reloaded.objects.size(), compiled.objects.size());
        for (int i = 0; i < compiled.objects.size(); ++i) {
            // Порядок пар кернинга в XML не меняет их смысл.
            QCOMPARE(reloaded.objects[i].type, compiled.objects[i].type);
            QCOMPARE(reloaded.objects[i].iparams, compiled.objects[i].iparams);
            QCOMPARE(reloaded.objects[i].data, compiled.objects[i].data);
        }
        QCOMPARE(reloaded.fonts.keys(), compiled.fonts.keys());
        for (const int index : compiled.fonts.keys()) {
            QCOMPARE(reloaded.fonts[index].maskData, compiled.fonts[index].maskData);
            QCOMPARE(reloaded.fonts[index].kerningPairs, compiled.fonts[index].kerningPairs);
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
            QVERIFY(dynamic_cast<ImageObject *>(p->getObjectAt(0).data())->isRotationObject());
            QVERIFY(!dynamic_cast<ImageObject *>(p->getObjectAt(1).data())->isRotationObject());
        }
    }
    void textLayoutAndStaticPackets() {
        auto doc = textFixture();
        const auto original = doc.toByteArray();
        QString error;
        const auto source = FpgaCompiledDocument::fromDomDocument(doc, &error);
        QVERIFY2(source.isValid(), qPrintable(error));
        TextLayout layout;
        QVERIFY(layoutText("AVA A", source.fonts[0], &layout, &error));
        QCOMPARE(layout.size, QSize(20,4));
        QCOMPARE(layout.glyphs.size(), 4);
        const QList<QPoint> expected = {{0,0},{5,1},{6,0},{14,0}};
        for (int i = 0; i < expected.size(); ++i) QCOMPARE(layout.glyphs[i].position, expected[i]);

        // Тексты одного шрифта разделены другим объектом: порядок слоёв сохраняется.
        auto objects = doc.documentElement().firstChildElement("objects");
        auto rectangle = doc.createElement("rectangle");
        auto init = doc.createElement("init"); init.appendChild(doc.createTextNode(testInit("rectangle", {{"enb",1},{"color",0xabcdef},{"w",1},{"h",1}})));
        rectangle.appendChild(init); objects.appendChild(rectangle);
        auto secondText = objects.firstChildElement("text").cloneNode(true).toElement(); objects.appendChild(secondText);
        const auto bundle = compileTextFixture(doc, &error);
        QVERIFY2(bundle.document.isValid(), qPrintable(error));
        QCOMPARE(bundle.document.objects.size(), 9);
        QCOMPARE(bundle.document.objects[4].type, QString("rectangle"));
        QCOMPARE(bundle.document.memories.size(), 1);
        QCOMPARE(bundle.document.memories[0].data, QString("70737076502707717"));
        QVERIFY(!bundle.document.schemas.contains("text")); QVERIFY(!bundle.document.schemas.contains("font"));
        const QList<int> addresses = {0,6,0,0};
        int memoryPackets = 0, parameterPackets = 0;
        QByteArray binary;
        for (const auto &packet : bundle.packets) {
            binary += packet.bytes;
            QCOMPARE(quint8(packet.bytes[6]), quint8(packet.type));
            QCOMPARE(packet.bytes.right(4).toHex(), QByteArray::number(FpgaPacketFramer::fletcher32(packet.bytes.left(packet.bytes.size()-4)),16).rightJustified(8,'0'));
            if (packet.type == FpgaPacketType::StaticMemory) {
                ++memoryPackets; QCOMPARE(packet.memId,0); QCOMPARE(packet.address,0); QCOMPARE(packet.payloadSize,17);
                QCOMPARE(packet.bytes.mid(12,17), QByteArray::fromHex("0700070307000706050002070007070107"));
            } else {
                QCOMPARE(packet.type,FpgaPacketType::Parameter);
                QCOMPARE(packet.objId, parameterPackets++);
                QCOMPARE(packet.payloadSize,19);
                QVERIFY(packet.objectType == "staticgroup" || packet.objectType == "rectangle");
            }
        }
        QCOMPARE(memoryPackets,1); QCOMPARE(parameterPackets,9);
        for (int i = 0; i < 4; ++i) {
            const auto &object = bundle.document.objects[i];
            QCOMPARE(object.memId,0);
            const auto hex = object.iparams.first();
            QCOMPARE(BitParser::extract(hex,25,12), quint32(10+expected[i].x()));
            QCOMPARE(BitParser::extract(hex,37,12), quint32(20+expected[i].y()));
            QCOMPARE(BitParser::extract(hex,73,16), quint32(addresses[i]));
            QCOMPARE(BitParser::extract(hex,1,24), quint32(0x563412));
            QCOMPARE(hex.left(15), QString(15,'0')); // Резерв старших битов обнулён.
        }
        QTemporaryDir temp;
        QVERIFY(FpgaPacketDump::writeBinary(temp.filePath("text.bin"),bundle,&error));
        QFile file(temp.filePath("text.bin")); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),binary);
        QCOMPARE(FpgaUartTransport::sceneBytes(bundle),binary);
        QCOMPARE(textFixture().toByteArray(),original);

        // Симулятор использует реальные байты, а не init/data из сопроводительной модели.
        FpgaSimulator simulator; simulator.loadBundle(bundle); const auto frame = simulator.renderFrame();
        QCOMPARE(frame.pixelColor(10,20), QColor(0x12,0x34,0x56));
        auto withoutMetadata = bundle;
        withoutMetadata.document.memories[0].data.fill('0');
        for (auto &object : withoutMetadata.document.objects) object.iparams.fill(QString(38,'0'));
        simulator.loadBundle(withoutMetadata); QCOMPARE(simulator.renderFrame(),frame);
        withoutMetadata.packets.clear(); simulator.loadBundle(withoutMetadata);
        QVERIFY(simulator.renderFrame() != frame);

        auto otherFont = objects.firstChildElement("font").cloneNode(true).toElement();
        otherFont.setAttribute("index",1); objects.appendChild(otherFont);
        for (auto glyph=otherFont.firstChildElement("init"); !glyph.isNull(); glyph=glyph.nextSiblingElement("init")) {
            auto h=glyph.text(); BitParser::inject(h,137,8,1); glyph.firstChild().setNodeValue(h);
        }
        auto otherText=secondText.cloneNode(true).toElement(); objects.appendChild(otherText);
        auto otherInit=otherText.firstChildElement("init"); auto h=otherInit.text(); BitParser::inject(h,49,8,1); otherInit.firstChild().setNodeValue(h);
        const auto twoFonts=compileTextFixture(doc,&error); QVERIFY2(twoFonts.document.isValid(),qPrintable(error));
        QCOMPARE(twoFonts.document.memories.size(),2);
        QCOMPARE(twoFonts.document.objects.last().memId,1); QCOMPARE(twoFonts.document.objects.first().memId,0);
        int uploads=0; for (const auto &packet:twoFonts.packets) if(packet.type==FpgaPacketType::StaticMemory) ++uploads;
        QCOMPARE(uploads,2);
    }
    void textClippingSpacesAndOffsets() {
        QString error;
        auto doc = textFixture("AA",9,8);
        doc.documentElement().setAttribute("width",11); doc.documentElement().setAttribute("height",10);
        auto objects = doc.documentElement().firstChildElement("objects");
        objects.appendChild(objects.firstChildElement("text").cloneNode(true));
        const auto bundle = compileTextFixture(doc,&error);
        QVERIFY2(bundle.document.isValid(),qPrintable(error));
        QCOMPARE(bundle.document.objects.size(),2); // Вторые A полностью за экраном.
        QCOMPARE(bundle.document.memories[0].data, QString("70737076502707717")+"7037");
        for (const auto &object : bundle.document.objects) {
            const auto hex = object.iparams.first();
            QCOMPARE(BitParser::extract(hex,49,12),quint32(2)); QCOMPARE(BitParser::extract(hex,61,12),quint32(2));
            QCOMPARE(BitParser::extract(hex,73,16),quint32(17));
        }
        FpgaSimulator simulator; simulator.loadBundle(bundle); const auto image = simulator.renderFrame();
        QCOMPARE(image.pixelColor(10,9),QColor(0x12,0x34,0x56)); // Вторая строка обрезанного растра.

        auto empty = compileTextFixture(textFixture("   "),&error);
        QVERIFY2(empty.document.isValid(),qPrintable(error)); QVERIFY(empty.document.objects.isEmpty());
        QCOMPARE(empty.packets.size(),1); QCOMPARE(empty.packets.first().type,FpgaPacketType::StaticMemory);
        doc = textFixture(QStringLiteral("AЯV"));
        auto init = doc.documentElement().firstChildElement("objects").firstChildElement("text").firstChildElement("init");
        auto hex = init.text(); BitParser::inject(hex,57,16,1); BitParser::inject(hex,73,12,1); BitParser::inject(hex,0,1,0);
        init.firstChild().setNodeValue(hex);
        const auto disabled = compileTextFixture(doc,&error); QVERIFY2(disabled.document.isValid(),qPrintable(error));
        QCOMPARE(disabled.document.objects.size(),1);
        QCOMPARE(BitParser::extract(disabled.document.objects.first().iparams.first(),73,16),quint32(13));
        QCOMPARE(BitParser::extract(disabled.document.objects.first().iparams.first(),0,1),quint32(0));
    }
    void textAddressLimitsAndInvalidXml() {
        QString error;
        auto source = FpgaCompiledDocument::fromDomDocument(textFixture("A"),&error);
        source.fonts[0].maskData = QString(65530,'0') + "707370";
        source.fonts[0].glyphs['A'].offset = 65530;
        auto scene = FpgaSceneCompiler().compile(source,&error); QVERIFY2(scene.isValid(),qPrintable(error));
        QCOMPARE(BitParser::extract(scene.objects.first().iparams.first(),73,16),quint32(65530));
        const auto bundle = FpgaPacketCompiler().compile(scene,&error); QVERIFY2(bundle.document.isValid(),qPrintable(error));
        QCOMPARE(bundle.packets.last().address,64512); QCOMPARE(bundle.packets.last().payloadSize,1024);
        source.height=21; // Усечение последней строки не требует копии в уже полном банке.
        scene=FpgaSceneCompiler().compile(source,&error); QVERIFY2(scene.isValid(),qPrintable(error));
        QCOMPARE(scene.memories.first().data.size(),65536);
        QCOMPARE(BitParser::extract(scene.objects.first().iparams.first(),61,12),quint32(1));
        source.fonts[0].maskData.append('0');
        QVERIFY(!FpgaSceneCompiler().compile(source,&error).isValid()); QVERIFY(error.contains("65536"));
        source = FpgaCompiledDocument::fromDomDocument(textFixture("AAA"),&error);
        source.width = 12000; source.fonts[0].glyphs['A'].advance = 4095;
        QVERIFY(!FpgaSceneCompiler().compile(source,&error).isValid()); QVERIFY(error.contains("12-бит"));
        source = FpgaCompiledDocument::fromDomDocument(textFixture(),&error);
        for (int i = 1; i <= 256; ++i) source.fonts.insert(i,source.fonts[0]);
        QVERIFY(!FpgaSceneCompiler().compile(source,&error).isValid()); QVERIFY(error.contains("256"));
        source.fonts.clear(); source.objects.clear();
        FpgaCompiledObject many; many.type="rectangle"; many.iparams.fill(QString(38,'0'),65537); source.objects.append(many);
        QVERIFY(!FpgaSceneCompiler().compile(source,&error).isValid()); QVERIFY(error.contains("65536"));

        for (int failure = 0; failure < 6; ++failure) {
            auto doc = textFixture();
            auto objects = doc.documentElement().firstChildElement("objects");
            auto font = objects.firstChildElement("font");
            auto glyph = font.firstChildElement("init");
            auto textInit = objects.firstChildElement("text").firstChildElement("init");
            if (failure == 0) font.firstChildElement("data").firstChild().setNodeValue("9");
            if (failure == 1) { auto h=glyph.text(); BitParser::inject(h,101,24,999); glyph.firstChild().setNodeValue(h); }
            if (failure == 2) { auto h=glyph.text(); BitParser::inject(h,125,12,5); glyph.firstChild().setNodeValue(h); }
            if (failure == 3) { auto h=textInit.text(); BitParser::inject(h,49,8,1); textInit.firstChild().setNodeValue(h); }
            if (failure == 4) { auto h=textInit.text(); BitParser::inject(h,73,12,99); textInit.firstChild().setNodeValue(h); }
            if (failure == 5) objects.firstChildElement("text").firstChildElement("data").firstChild().setNodeValue("65536");
            const auto invalid = compileTextFixture(doc,&error);
            QVERIFY(!error.isEmpty()); QVERIFY(!invalid.document.isValid()); QVERIFY(invalid.packets.isEmpty());
        }
        QVERIFY(!compileTextFixture(textFixture("Z"),&error).document.isValid()); QVERIFY(error.contains("U+005A"));
    }
    void textPreviewXmlAndUart() {
        auto *p = ProjectManager::instance(); p->createNewProject("Text",320,160,QColor(20,30,40));
        const int index = p->addObject("text");
        auto *text = dynamic_cast<TextObject *>(p->getObjectAt(index).data()); QVERIFY(text);
        QVERIFY(text->setObjectProperty("Текст",QStringLiteral("AVAV Яя gyp α° 11")));
        text->states[0].x = 5; text->states[0].y = 7;
        QString error;
        const auto bundle = FpgaProjectPacketBuilder::buildCurrentProject(&error); QVERIFY2(bundle.document.isValid(),qPrintable(error));
        const auto dom = p->buildCompiledFpgaXml();
        QCOMPARE(dom.documentElement().firstChildElement("objects").elementsByTagName("text").size(),1);
        const auto source = FpgaCompiledDocument::fromDomDocument(dom,&error);
        TextLayout layout; QVERIFY(layoutText(text->text,source.fonts.first(),&layout,&error));
        QCOMPARE(text->states.first().w,layout.size.width()); QCOMPARE(text->states.first().h,layout.size.height());
        QImage preview(320,160,QImage::Format_ARGB32); preview.fill(QColor(20,30,40));
        { QPainter painter(&preview); painter.drawImage(5,7,text->maskImages.first()); }
        FpgaSimulator simulator; simulator.loadBundle(bundle); const auto frame = simulator.renderFrame();
        int maxDifference = 0;
        for (int y=0;y<160;++y) for (int x=0;x<320;++x) {
            const auto a=preview.pixelColor(x,y), b=frame.pixelColor(x,y);
            maxDifference=qMax(maxDifference,qMax(qAbs(a.red()-b.red()),qMax(qAbs(a.green()-b.green()),qAbs(a.blue()-b.blue()))));
        }
        QVERIFY2(maxDifference<=3,qPrintable(QString::number(maxDifference)));
        const auto bytes = FpgaUartTransport::sceneBytes(bundle);
        QTemporaryDir temp; QVERIFY(p->saveToFile(temp.filePath("text.xml"))); QVERIFY2(p->loadFromFile(temp.filePath("text.xml")),qPrintable(p->lastErrorMessage()));
        const auto restored = FpgaProjectPacketBuilder::buildCurrentProject(&error); QVERIFY2(restored.document.isValid(),qPrintable(error));
        QCOMPARE(FpgaUartTransport::sceneBytes(restored),bytes);
        // До упаковки в XML координаты и количество символов не должны молча обрезаться.
        text = dynamic_cast<TextObject *>(p->getObjectAt(0).data()); text->states[0].x=-1;
        QVERIFY(!FpgaProjectPacketBuilder::buildCurrentProject(&error).document.isValid()); QVERIFY(!error.isEmpty());
        text->states[0].x=0; text->text=QString(4096,'A');
        QVERIFY(!FpgaProjectPacketBuilder::buildCurrentProject(&error).document.isValid()); QVERIFY(!error.isEmpty());
#ifdef Q_OS_UNIX
        PtyCapture capture; QVERIFY(capture.master>=0);
        FpgaUartTransport sender; FpgaUartConfig config; config.portName=QString::fromLocal8Bit(capture.name);
        config.loadBaudRate=57600; config.baudRate=115200;
        QSignalSpy failed(&sender,&FpgaUartTransport::failed);
        QVERIFY(sender.startStreaming(restored,config,&error));
        const auto expected=QByteArray(1,char(0))+bytes;
        QTRY_VERIFY_WITH_TIMEOUT(capture.data().size()>=expected.size() || !failed.isEmpty(),3000);
        sender.stopStreaming(); QVERIFY(failed.isEmpty()); QCOMPARE(capture.data(),expected);
#endif
    }
    void rectangleCornersAndTransparency() {
        RectangleObject rectangle;
        rectangle.x=50; rectangle.y=50; rectangle.width=80; rectangle.height=60;
        rectangle.fillColor=QColor("#00aaf0"); rectangle.strokeColor=Qt::white; rectangle.strokeWidth=20;
        const auto parameters=rectangle.serializeParams();
        QImage image(180,160,QImage::Format_ARGB32); image.fill(Qt::black);
        { QPainter painter(&image); painter.setRenderHint(QPainter::Antialiasing); rectangle.draw(painter); }
        QCOMPARE(image.pixelColor(31,31),QColor(Qt::white));
        QCOMPARE(image.pixelColor(149,129),QColor(Qt::white));
        QCOMPARE(image.pixelColor(29,29),QColor(Qt::black));
        QCOMPARE(image.pixelColor(51,51),rectangle.fillColor);
        QCOMPARE(rectangle.serializeParams(),parameters);
        image.save(QCoreApplication::applicationDirPath()+"/rectangle-corners.png");
        QVERIFY(rectangle.setObjectProperty("Прозрачность","50"));
        QCOMPARE(rectangle.alpha,128); QCOMPARE(rectangle.serializeParams()["alph"],quint32(3));
        QVERIFY(!rectangle.setObjectProperty("Прозрачность","101")); QCOMPARE(rectangle.alpha,128);
        QVERIFY(rectangle.setObjectProperty("Прозрачность","100"));
        QCOMPARE(rectangle.serializeParams()["enb"],quint32(0));
        RectangleObject restored;
        restored.parse(testInit("rectangle_a",rectangle.serializeParams()),FpgaSchemaRegistry::instance()->buildSchema("rectangle_a"));
        QCOMPARE(restored.alpha,0);
        QVERIFY(restored.setObjectProperty("Прозрачность","0")); QCOMPARE(restored.serializeParams()["enb"],quint32(1));

        auto *p=ProjectManager::instance(); p->createNewProject("Transparency",640,480,Qt::black);
        const int index=p->addObject("rectangle");
        ObjectPropertiesPanel panel; panel.resize(340,540); panel.show(); panel.showObjectProperties(index); QTest::qWait(5);
        auto *spin=panel.findChild<QSpinBox *>("TransparencySpin"); auto *slider=panel.findChild<QSlider *>("TransparencySlider");
        QVERIFY(spin && slider); QCOMPARE(spin->maximum(),100); QCOMPARE(slider->maximum(),100);
        QCOMPARE(spin->value(),0); QCOMPARE(spin->text(),QString("0 %")); QVERIFY(spin->specialValueText().isEmpty());
        auto *layerCombo = panel.findChild<QComboBox *>("ObjectLayerCombo"); QVERIFY(layerCombo);
        QCOMPARE(layerCombo->height(), spin->height());
        EditorWorkspacePanel workspace; workspace.resize(800,500); workspace.show(); QTest::qWait(5);
        auto *workspaceCombo = workspace.findChild<QComboBox *>("WorkspaceLayerCombo"); QVERIFY(workspaceCombo);
        QCOMPARE(workspaceCombo->height(), spin->height());
        workspace.grab().save(QCoreApplication::applicationDirPath()+"/workspace-layer-field.png");
        const auto *properties=panel.findChild<QTableWidget *>("PropertiesTable"); QVERIFY(properties);
        for (int row=0;row<properties->rowCount();++row)
            if (properties->cellWidget(row,1)) QCOMPARE(properties->rowHeight(row),properties->rowHeight(0));
        panel.grab().save(QCoreApplication::applicationDirPath()+"/transparency-controls.png");
        QVERIFY2(spin->parentWidget()->rect().contains(spin->geometry()),qPrintable(QString("editor %1x%2; spin %3,%4 %5x%6; hint %7; minimum %8")
            .arg(spin->parentWidget()->width()).arg(spin->parentWidget()->height()).arg(spin->x()).arg(spin->y()).arg(spin->width()).arg(spin->height()).arg(spin->parentWidget()->sizeHint().height()).arg(spin->minimumSizeHint().height())));
        const auto *lineEdit=spin->findChild<QLineEdit *>(); QVERIFY(lineEdit);
        QVERIFY(spin->rect().contains(QRect(lineEdit->mapTo(spin,QPoint()),lineEdit->size())));
        QVERIFY(slider->width()>=24);
        spin->setValue(50); QCOMPARE(dynamic_cast<RectangleObject *>(p->getObjectAt(index).data())->alpha,128);
        spin->setValue(0);
        slider->sliderPressed(); slider->setValue(25); slider->setValue(50); slider->setValue(75); slider->sliderReleased();
        QCOMPARE(dynamic_cast<RectangleObject *>(p->getObjectAt(index).data())->alpha,64);
        QVERIFY(p->undo()); QCOMPARE(dynamic_cast<RectangleObject *>(p->getObjectAt(index).data())->alpha,255);
        QVERIFY(p->redo());
        panel.showObjectProperties(index); QTest::qWait(5);
        panel.grab().save(QCoreApplication::applicationDirPath()+"/transparency-controls.png");
        const auto bundle=FpgaProjectPacketBuilder::buildCurrentProject(); QVERIFY(bundle.document.isValid());
        FpgaSimulator simulator; simulator.loadBundle(bundle); const auto frame=simulator.renderFrame();
        const auto rect=dynamic_cast<RectangleObject *>(p->getObjectAt(0).data())->getBoundingRect();
        QVERIFY(frame.pixelColor(qRound(rect.center().x()),qRound(rect.center().y()))!=QColor(Qt::black));
    }
    void mergeStaticGroupsAndUndo() {
        auto *p=ProjectManager::instance(); p->createNewProject("Merge",320,160,Qt::black);
        QTemporaryDir temp;
        for (int i=0;i<3;++i) {
            QImage image(16,16,QImage::Format_ARGB32); image.fill(QColor::fromHsv(i*120,255,255));
            const auto path=temp.filePath(QString::number(i)+".png"); QVERIFY(image.save(path)); QVERIFY(p->importImage(path)>=0);
        }
        for (int i=0;i<3;++i) {
            auto *image=dynamic_cast<ImageObject *>(p->getObjectAt(i).data()); image->x=i<2?20:100; image->y=30;
        }
        auto render=[&](){
            QImage frame(320,160,QImage::Format_ARGB32); frame.fill(Qt::black); QPainter painter(&frame);
            for (auto it=p->getObjects().crbegin();it!=p->getObjects().crend();++it) (*it)->draw(painter);
            return frame;
        };
        const auto before=render(); QString error;
        const int mergedIndex=p->mergeStaticGroups({2,0,1,1},&error); QVERIFY2(mergedIndex>=0,qPrintable(error));
        QCOMPARE(p->getObjectCount(),1);
        auto *merged=dynamic_cast<StaticGroupObject *>(p->getObjectAt(0).data()); QVERIFY(merged);
        QCOMPARE(merged->states.size(),3); QCOMPARE(merged->states[1].addr,256); QCOMPARE(merged->states[2].addr,512);
        QCOMPARE(render(),before);
        const auto objects=p->buildCompiledFpgaXml().documentElement().firstChildElement("objects");
        QCOMPARE(objects.elementsByTagName("staticgroup").size(),1); QCOMPARE(objects.elementsByTagName("init").size(),3);
        const auto bundle=FpgaProjectPacketBuilder::buildCurrentProject(&error); QVERIFY2(bundle.document.isValid(),qPrintable(error));
        QCOMPARE(bundle.document.memories.size(),1); QCOMPARE(bundle.document.memories[0].data.size(),768);
        QVERIFY(p->undo()); QCOMPARE(p->getObjectCount(),3); QVERIFY(dynamic_cast<ImageObject *>(p->getObjectAt(0).data()));
        QVERIFY(p->redo()); QCOMPARE(p->getObjectCount(),1); QCOMPARE(render(),before);
        merged=dynamic_cast<StaticGroupObject *>(p->getObjectAt(0).data());
        QVERIFY(p->setObjectProperty(merged,"Состояние 0: Ширина","20"));
        QCOMPARE(merged->maskImages[0].size(),QSize(20,16)); QCOMPARE(merged->states[1].addr,320);
        QVERIFY(p->undo()); QCOMPARE(render(),before);
        QVERIFY(p->saveToFile(temp.filePath("merged.xml"))); QVERIFY(p->loadFromFile(temp.filePath("merged.xml")));
        merged=dynamic_cast<StaticGroupObject *>(p->getObjectAt(0).data()); QVERIFY(merged); QCOMPARE(merged->states.size(),3);
        QCOMPARE(render(),before);

        p->createNewProject("Eight init",640,480,Qt::black); p->addObject("staticgroup"); p->addObject("staticgroup");
        for (int index=0;index<2;++index) {
            auto *group=dynamic_cast<StaticGroupObject *>(p->getObjectAt(index).data());
            group->states.clear(); group->maskImages.clear();
            for (int i=0;i<4;++i) {
                group->states.append({index*120+i*20,30,16,16,i*256,Qt::white,true});
                QImage mask(16,16,QImage::Format_ARGB32); mask.fill(Qt::white); group->maskImages.append(mask);
            }
        }
        QVERIFY2(p->mergeStaticGroups({0,1},&error)>=0,qPrintable(error));
        merged=dynamic_cast<StaticGroupObject *>(p->getObjectAt(0).data()); QCOMPARE(merged->states.size(),8);
        QVERIFY(p->undo());
        auto *group=dynamic_cast<StaticGroupObject *>(p->getObjectAt(0).data()); group->states.append(group->states.first()); group->maskImages.append(group->maskImages.first());
        const auto original=p->getObjectAt(0);
        QVERIFY(p->mergeStaticGroups({0,1},&error)<0); QVERIFY(error.contains("8")); QCOMPARE(p->getObjectCount(),2); QCOMPARE(p->getObjectAt(0),original);
        group->states.resize(1); group->maskImages.resize(1); group->states[0].w=400; group->states[0].h=400;
        QVERIFY(p->mergeStaticGroups({0,1},&error)<0); QVERIFY(error.contains("65536")); QCOMPARE(p->getObjectCount(),2);
        group->states[0].w=16; group->states[0].h=16;
        p->addObject("text"); QVERIFY(p->mergeStaticGroups({0,1},&error)<0); QVERIFY(error.contains("текст"));
        p->createNewProject("Full static bank",640,480,Qt::black); p->addObject("staticgroup"); p->addObject("staticgroup");
        for (int i=0;i<2;++i) {
            auto *part=dynamic_cast<StaticGroupObject *>(p->getObjectAt(i).data());
            part->states={{20,20+i*140,256,128,0,Qt::white,true}};
            QImage mask(256,128,QImage::Format_ARGB32); mask.fill(Qt::white); part->maskImages={mask};
        }
        QVERIFY2(p->mergeStaticGroups({0,1},&error)>=0,qPrintable(error));
        merged=dynamic_cast<StaticGroupObject *>(p->getObjectAt(0).data()); QCOMPARE(merged->states[1].addr,32768);
        const auto full=FpgaProjectPacketBuilder::buildCurrentProject(&error); QVERIFY2(full.document.isValid(),qPrintable(error));
        QCOMPARE(full.document.memories.first().data.size(),65536);
    }
    void rasterMemoryWarnings() {
        auto *p=ProjectManager::instance(); p->createNewProject("Memory",1024,768,Qt::black); const int index=p->addObject("staticgroup");
        auto *group=dynamic_cast<StaticGroupObject *>(p->getObjectAt(index).data());
        group->states[0].w=300; group->states[0].h=300; group->maskImages[0]=QImage(300,300,QImage::Format_ARGB32); group->maskImages[0].fill(Qt::white);
        QVERIFY(!FpgaResourceLimits::inspect(*group).errors.isEmpty());
        ObjectPropertiesPanel panel; panel.resize(360,580); panel.show(); panel.showObjectProperties(index); QTest::qWait(5);
        auto *warning=panel.findChild<QLabel *>("PropertiesHardwareWarning"); QVERIFY(warning); QVERIFY(!warning->isHidden()); QVERIFY(warning->text().contains("90000/65536"));
        panel.grab().save(QCoreApplication::applicationDirPath()+"/memory-warning.png");
        QString error; QVERIFY(!FpgaProjectPacketBuilder::buildCurrentProject(&error).document.isValid()); QVERIFY(error.contains("65536"));
        QTemporaryDir temp; QFile file(temp.filePath("keep.xml")); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("keep"); file.close();
        QVERIFY(!p->saveToFile(file.fileName())); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),QByteArray("keep")); file.close();
        group->states[0].w=32; group->states[0].h=32; group->changed(); QVERIFY(warning->isHidden());

        p->createNewProject("Rotation memory",1024,768,Qt::black); p->addObject("rotationobject");
        auto *rotation=dynamic_cast<RotationObject *>(p->getObjectAt(0).data()); rotation->maskImage=QImage(300,600,QImage::Format_ARGB32); rotation->maskImage.fill(Qt::white);
        QVERIFY(!FpgaResourceLimits::inspect(*rotation).errors.isEmpty());
        QVERIFY(!FpgaProjectPacketBuilder::buildCurrentProject(&error).document.isValid()); QVERIFY(error.contains("2048"));
        QImage image(20,20,QImage::Format_ARGB32); image.fill(Qt::white); QVERIFY(image.save(temp.filePath("large.png")));
        p->createNewProject("Simplified",1024,768,Qt::black); QVERIFY(p->importImage(temp.filePath("large.png"),true)>=0);
        auto *source=dynamic_cast<ImageObject *>(p->getObjectAt(0).data()); source->width=500; source->height=500;
        const auto report=FpgaResourceLimits::inspect(*source); QVERIFY(report.errors.isEmpty()); QVERIFY(!report.warnings.isEmpty());
        const auto bundle=FpgaProjectPacketBuilder::buildCurrentProject(&error); QVERIFY2(bundle.document.isValid(),qPrintable(error)); QVERIFY(!bundle.warnings.isEmpty());
    }
    void transformModifiersAndMenus() {
        auto *p=ProjectManager::instance(); p->createNewProject("Modifiers",640,480,Qt::black); p->addObject("rectangle");
        p->setSnapToCanvas(false); p->setSnapToGrid(false); p->setSnapToObjects(false);
        auto *rectangle=dynamic_cast<RectangleObject *>(p->getObjectAt(0).data()); rectangle->x=60; rectangle->y=70; rectangle->width=100; rectangle->height=50;
        ViewportPanel viewport; viewport.resize(640,480); viewport.show(); viewport.setSelectedIndex(0); QTest::qWait(5);
        auto point=[](double x,double y){return QPoint(qRound(16+x*.95),qRound(12+y*.95));};
        auto move=[&](QPoint position,Qt::KeyboardModifiers modifiers){
            QMouseEvent event(QEvent::MouseMove,QPointF(position),QPointF(viewport.mapToGlobal(position)),Qt::NoButton,Qt::LeftButton,modifiers);
            QApplication::sendEvent(&viewport,&event);
        };
        QTest::mousePress(&viewport,Qt::LeftButton,Qt::ShiftModifier,point(160,120));
        move(point(240,130),Qt::ShiftModifier); QTest::mouseRelease(&viewport,Qt::LeftButton,Qt::ShiftModifier,point(240,130));
        QVERIFY(qAbs(rectangle->width/rectangle->height-2)<.001); QVERIFY(rectangle->width>150); QCOMPARE(viewport.getSelectedIndexes(),QList<int>{0});
        QVERIFY(p->undo()); rectangle=dynamic_cast<RectangleObject *>(p->getObjectAt(0).data()); QCOMPARE(rectangle->width,100.); QCOMPARE(rectangle->height,50.);
        QTemporaryDir temp; QImage raster(16,16,QImage::Format_ARGB32); raster.fill(Qt::white); QVERIFY(raster.save(temp.filePath("rotate.png")));
        QVERIFY(p->importImage(temp.filePath("rotate.png"),true)>=0);
        auto *image=dynamic_cast<ImageObject *>(p->getObjectAt(0).data()); image->x=200; image->y=150; image->width=100; image->height=50;
        viewport.setSelectedIndex(0);
        for (const auto modifier : {Qt::ShiftModifier,Qt::ControlModifier}) {
            QTest::mousePress(&viewport,Qt::LeftButton,modifier,point(250,150-24/.95));
            move(point(300,120),modifier); QTest::mouseRelease(&viewport,Qt::LeftButton,modifier,point(300,120));
            const double step=modifier==Qt::ShiftModifier?15.:90.;
            QVERIFY(qAbs(image->rotationDegrees/step-qRound(image->rotationDegrees/step))<.001);
            QCOMPARE(image->x,200.); QCOMPARE(image->y,150.);
        }
        MainWindow window;
        auto *merge=window.findChild<QAction *>("MergeStaticAction"); QVERIFY(merge); QCOMPARE(merge->shortcut(),QKeySequence("Ctrl+Alt+G"));
        QVERIFY(window.findChild<QAction *>("ProportionalResizeAction")); QVERIFY(window.findChild<QAction *>("SteppedRotationAction"));
        QVERIFY(window.findChild<QToolButton *>("MergeStaticButton"));
        SettingsWindow settings; int shortcuts=0;
        for(auto *label:settings.findChildren<QLabel *>()) if(label->text()=="Ctrl+Alt+G" || label->text()=="Ctrl+Alt+M" || label->text()=="Ctrl+Shift+P" || label->text()=="Ctrl+Shift+R") ++shortcuts;
        QCOMPARE(shortcuts,4);
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
        auto *mergeButton = strip->findChild<QToolButton *>("MergeStaticButton"); QVERIFY(mergeButton);
        auto *neighbor = strip->findChild<QToolButton *>("SelectionToolButton"); QVERIFY(neighbor);
        QCOMPARE(mergeButton->size(),neighbor->size());
        QCOMPARE(mergeButton->iconSize(),neighbor->iconSize());
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
