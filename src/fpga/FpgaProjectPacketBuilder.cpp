//FpgaProjectPacketBuilder - единая точка сборки текущего проекта в пакетный bundle

#include "fpga/FpgaProjectPacketBuilder.h"

#include "ProjectManager.h"
#include "fpga/model/FpgaCompiledDocument.h"
#include "fpga/packets/FpgaPacketCompiler.h"

FpgaPacketBundle FpgaProjectPacketBuilder::buildCurrentProject(QString *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();

    const QDomDocument dom = ProjectManager::instance()->buildCompiledFpgaXml();
    QString parseError;
    const FpgaCompiledDocument document = FpgaCompiledDocument::fromDomDocument(dom, &parseError);
    if (!parseError.isEmpty()) {
        if (errorMessage)
            *errorMessage = parseError;
        return {};
    }

    FpgaPacketCompiler compiler;
    return compiler.compile(document);
}

