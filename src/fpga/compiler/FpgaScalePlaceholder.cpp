#include "FpgaScalePlaceholder.h"

QStringList FpgaScalePlaceholder::parameters(bool firstScale)
{
    // Утверждённый образ 450'h0000000000000000000000107000106e00106c0010dc0010500010a000007d0000000000301c00001ff0074666600164ccd08c00dc011849f.
    // Границы 150-битных слов не совпадают с границами HEX-цифр.
    return {
        firstScale ? QStringLiteral("000001ff0074666600164ccd08c00dc011849f")
                   : QStringLiteral("000001ff0074666600164ccd08c00dc011849e"),
        QStringLiteral("00043700041400042800001f40000000000c07"),
        QStringLiteral("0000000000000000000000107000106e00106c")
    };
}
