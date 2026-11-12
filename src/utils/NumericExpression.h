// NumericExpression - безопасное вычисление арифметики в числовых полях редактора.

#pragma once

#include <QString>
#include <QtMath>
#include <utility>

namespace NumericExpression {
// Parser - разбор +, -, *, /, скобок и унарных знаков без исполнения кода.
class Parser
{
public:
    explicit Parser(QString text) : m_text(std::move(text))
    {
        m_text.replace(QChar(0x2212), '-');
        m_text.replace(QChar(0x00d7), '*');
        m_text.replace(QChar(0x00f7), '/');
        m_text.replace(',', '.');
    }
    bool evaluate(double *result)
    {
        if (!result || m_text.size() > 1024) return false;
        take('=');
        const double value = expression();
        skipSpace();
        if (!m_ok || m_pos != m_text.size() || !qIsFinite(value)) return false;
        *result = value;
        return true;
    }
private:
    void skipSpace() { while (m_pos < m_text.size() && m_text[m_pos].isSpace()) ++m_pos; }
    bool take(char ch)
    {
        skipSpace();
        if (m_pos < m_text.size() && m_text[m_pos] == QLatin1Char(ch)) { ++m_pos; return true; }
        return false;
    }
    double expression()
    {
        double value = term();
        while (m_ok) {
            if (take('+')) value += term();
            else if (take('-')) value -= term();
            else break;
            if (!qIsFinite(value)) m_ok = false;
        }
        return value;
    }
    double term()
    {
        double value = factor();
        while (m_ok) {
            if (take('*')) value *= factor();
            else if (take('/')) {
                const double divisor = factor();
                if (divisor == 0.0) { m_ok = false; return 0.0; }
                value /= divisor;
            } else break;
            if (!qIsFinite(value)) m_ok = false;
        }
        return value;
    }
    double factor()
    {
        if (++m_depth > 64) { m_ok = false; return 0.0; }
        double value = 0.0;
        if (take('+')) value = factor();
        else if (take('-')) value = -factor();
        else if (take('(')) { value = expression(); if (!take(')')) m_ok = false; }
        else value = number();
        --m_depth;
        return value;
    }
    double number()
    {
        skipSpace();
        const int start = m_pos;
        auto digit = [this]() { return m_pos < m_text.size() && m_text[m_pos] >= QLatin1Char('0') && m_text[m_pos] <= QLatin1Char('9'); };
        while (digit()) ++m_pos;
        if (m_pos < m_text.size() && m_text[m_pos] == QLatin1Char('.')) {
            ++m_pos;
            while (digit()) ++m_pos;
        }
        if (m_pos < m_text.size() && (m_text[m_pos] == QLatin1Char('e') || m_text[m_pos] == QLatin1Char('E'))) {
            ++m_pos;
            if (m_pos < m_text.size() && (m_text[m_pos] == QLatin1Char('+') || m_text[m_pos] == QLatin1Char('-'))) ++m_pos;
            while (digit()) ++m_pos;
        }
        bool ok = false;
        const double value = m_text.mid(start, m_pos - start).toDouble(&ok);
        if (!ok || !qIsFinite(value)) m_ok = false;
        return value;
    }
    QString m_text;
    int m_pos = 0;
    int m_depth = 0;
    bool m_ok = true;
};

inline bool evaluate(const QString &text, double *result) { return Parser(text).evaluate(result); }
inline QString helpText() { return QStringLiteral("Можно вводить +, −, *, / и скобки, например (1920 - 80) / 2. Enter — вычислить."); }
}
