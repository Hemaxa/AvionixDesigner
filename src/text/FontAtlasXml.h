// FontAtlasXml - чтение и проверка шрифтового атласа из XML.

#pragma once

#include "text/FontAtlas.h"
#include "BaseObject.h"

class QDomElement;

// Читает текущую схему font и проверяет размеры масок, коды, адреса и кернинг.
// Результат записывается в ненулевой font только после успешной проверки ресурса.
bool readFontAtlas(const QDomElement &element, const ParamSchema &schema,
                   FpgaFont *font, QString *errorMessage = nullptr);
