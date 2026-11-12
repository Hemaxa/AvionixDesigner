// EditorGestures - сочетание установки центра вращения и его подпись для текущей ОС.

#pragma once

#include <QString>
#include <Qt>

namespace EditorGestures {
inline constexpr auto rotationPivotModifiers = Qt::ControlModifier | Qt::AltModifier;
inline QString rotationPivotShortcut()
{
#ifdef Q_OS_MACOS
    return QStringLiteral("⌥⌘ + ЛКМ");
#else
    return QStringLiteral("Ctrl+Alt + ЛКМ");
#endif
}
}
