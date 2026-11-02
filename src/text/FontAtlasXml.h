#pragma once

#include "text/FontAtlas.h"
#include "BaseObject.h"

class QDomElement;

bool readFontAtlas(const QDomElement &element, const ParamSchema &schema,
                   FpgaFont *font, QString *errorMessage = nullptr);
