#include "EditorProjectDocument.h"

#include <QtGlobal>

void EditorProjectDocument::clear()
{
    m_projectName = QStringLiteral("Untitled");
    m_canvasWidth = 0;
    m_canvasHeight = 0;
    m_backgroundColor = Qt::black;
}

bool EditorProjectDocument::hasCanvas() const
{
    return m_canvasWidth > 0 && m_canvasHeight > 0;
}

QString EditorProjectDocument::projectName() const
{
    return m_projectName;
}

void EditorProjectDocument::setProjectName(const QString &name)
{
    m_projectName = name.trimmed().isEmpty() ? QStringLiteral("Untitled") : name.trimmed();
}

int EditorProjectDocument::canvasWidth() const
{
    return m_canvasWidth;
}

int EditorProjectDocument::canvasHeight() const
{
    return m_canvasHeight;
}

void EditorProjectDocument::setCanvasSize(int width, int height)
{
    m_canvasWidth = qMax(1, width);
    m_canvasHeight = qMax(1, height);
}

QColor EditorProjectDocument::backgroundColor() const
{
    return m_backgroundColor;
}

void EditorProjectDocument::setBackgroundColor(const QColor &color)
{
    m_backgroundColor = color.isValid() ? color : Qt::black;
}
