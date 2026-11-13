#pragma once

#include <QStringList>

// FpgaScalePlaceholder - временный аппаратный образ шкалы из трёх слов по 150 бит.
// Геометрия редактора хранится отдельно: эти слова нельзя разбирать по старой схеме шкалы.
class FpgaScalePlaceholder
{
public:
    static constexpr int parameterCount = 3;

    // Первая шкала в аппаратном порядке получает enable=1, остальные enable=0.
    // Порядок слов: биты [149:0], [299:150], [449:300] исходного 450-битного образа.
    static QStringList parameters(bool firstScale);
};
