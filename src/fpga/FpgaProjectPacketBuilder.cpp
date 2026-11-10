// FpgaProjectPacketBuilder - единая точка сборки текущего проекта в пакетный bundle.

#include "fpga/FpgaProjectPacketBuilder.h"

#include "ProjectManager.h"
#include "ThreeDViewObject.h"
#include "TextObject.h"
#include "fpga/model/FpgaCompiledDocument.h"
#include "fpga/compiler/FpgaSceneCompiler.h"
#include "fpga/compiler/FpgaResourceLimits.h"
#include "fpga/packets/FpgaPacketCompiler.h"

FpgaPacketBundle FpgaProjectPacketBuilder::buildCurrentProject(QString *errorMessage, const FpgaHardwareScene *loaded)
{
    if (errorMessage)
        errorMessage->clear();

    // Проверяем исходные значения до XML: inject() обрезает их до размера поля.
    const auto &objects = ProjectManager::instance()->getObjects();
    QList<int> textCapacities;
    for (const auto &object : objects) {
        if (object->isExportEnabled()) {
            const auto report = FpgaResourceLimits::inspect(*object);
            if (!report.errors.isEmpty()) {
                if (errorMessage)
                    *errorMessage = report.errors.join('\n');
                return {};
            }
        }
        const auto *text = dynamic_cast<const TextObject *>(object.data());
        if (!text || !text->isExportEnabled())
            continue;
        textCapacities.append(text->symbolCapacity);
        const auto state = text->states.isEmpty() ? GroupState{} : text->states.first();
        if (text->text.size() > 4095 || state.x < 0 || state.x > 4095 || state.y < 0 || state.y > 4095) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Текст не помещается в формат XML: X и Y должны быть 0…4095, длина — не более 4095 кодов UTF-16.");
            return {};
        }
    }

    const QDomDocument dom = ProjectManager::instance()->buildCompiledFpgaXml();
    QString parseError;
    FpgaCompiledDocument document = FpgaCompiledDocument::fromDomDocument(dom, &parseError);
    if (!parseError.isEmpty()) {
        if (errorMessage)
            *errorMessage = parseError;
        return {};
    }

    // XML записывает аппаратные объекты в обратном порядке списка редактора.
    for (auto &object : document.objects)
        if (object.type == "text") object.symbolCapacity = textCapacities.takeLast();

    if (const auto *view = ProjectManager::instance()->threeDViewObject()) {
        for (auto &object : document.objects)
            if (object.type == QStringLiteral("layer3d")) object.leftAnchored = view->isLeftAnchored();
    }
    const auto scene = FpgaSceneCompiler().compile(document, &parseError, loaded);
    if (!scene.isValid()) {
        if (errorMessage)
            *errorMessage = parseError;
        return {};
    }
    FpgaPacketCompiler compiler;
    auto bundle = compiler.compile(scene, errorMessage);
    if (bundle.document.isValid()) {
        for (const auto &object : ProjectManager::instance()->getObjects())
            if (object->isExportEnabled())
                bundle.warnings += FpgaResourceLimits::inspect(*object).warnings;
    }
    return bundle;
}

