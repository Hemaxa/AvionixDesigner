// ExpressionSpinBox - числовые поля Qt с вычислением арифметики при подтверждении ввода.

#pragma once

#include "NumericExpression.h"
#include <QSpinBox>
#include <QDoubleSpinBox>

namespace NumericExpression {
inline QString withoutAffixes(QString text, const QString &prefix, const QString &suffix)
{
    if (!prefix.isEmpty() && text.startsWith(prefix)) text.remove(0, prefix.size());
    if (!suffix.isEmpty() && text.endsWith(suffix)) text.chop(suffix.size());
    return text.trimmed();
}
}

// ExpressionSpinBox - целочисленное поле; дробный результат округляется к ближайшему целому.
class ExpressionSpinBox : public QSpinBox
{
public:
    explicit ExpressionSpinBox(QWidget *parent = nullptr) : QSpinBox(parent)
    {
        setKeyboardTracking(false);
        setToolTip(NumericExpression::helpText());
    }
protected:
    QValidator::State validate(QString &text, int &pos) const override
    {
        if (!specialValueText().isEmpty() && text == specialValueText()) return QSpinBox::validate(text, pos);
        double result = 0.0;
        if (NumericExpression::evaluate(NumericExpression::withoutAffixes(text, prefix(), suffix()), &result)
            && result >= minimum() && result <= maximum()) return QValidator::Acceptable;
        return QValidator::Intermediate;
    }
    int valueFromText(const QString &text) const override
    {
        double result = 0.0;
        if (NumericExpression::evaluate(NumericExpression::withoutAffixes(text, prefix(), suffix()), &result)
            && result >= minimum() && result <= maximum()) return qRound(result);
        return value();
    }
};

// ExpressionDoubleSpinBox - дробное поле с тем же синтаксисом и ограничениями диапазона.
class ExpressionDoubleSpinBox : public QDoubleSpinBox
{
public:
    explicit ExpressionDoubleSpinBox(QWidget *parent = nullptr) : QDoubleSpinBox(parent)
    {
        setKeyboardTracking(false);
        setToolTip(NumericExpression::helpText());
    }
protected:
    QValidator::State validate(QString &text, int &pos) const override
    {
        if (!specialValueText().isEmpty() && text == specialValueText()) return QDoubleSpinBox::validate(text, pos);
        double result = 0.0;
        if (NumericExpression::evaluate(NumericExpression::withoutAffixes(text, prefix(), suffix()), &result)
            && result >= minimum() && result <= maximum()) return QValidator::Acceptable;
        return QValidator::Intermediate;
    }
    double valueFromText(const QString &text) const override
    {
        double result = 0.0;
        if (NumericExpression::evaluate(NumericExpression::withoutAffixes(text, prefix(), suffix()), &result)
            && result >= minimum() && result <= maximum()) return result;
        return value();
    }
};
