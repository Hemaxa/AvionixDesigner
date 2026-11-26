// AppearanceManager - менеджер внешнего вида приложения.

#include "AppearanceManager.h"
#include <QApplication>
#include <QFile>
#include <QDebug>
#include <QRegularExpression>
#include <QPainter>
#include <QDir>
#include <cmath>

namespace {
// Подстановка общих значений в QSS. Неизвестные имена и циклы — ошибка темы.
bool expandValues(QString &text, const QHash<QString, QString> &values)
{
  static const QRegularExpression reference(QStringLiteral(R"(\$\{([\w.]+)\})"));
  for (qsizetype pass = 0; pass <= values.size(); ++pass) {
    const auto match = reference.match(text);
    if (!match.hasMatch()) {
      static const QRegularExpression alpha(QStringLiteral(R"(alpha\((#[\da-fA-F]{6}),\s*(\d{1,3})\))"));
      auto tint = alpha.match(text);
      while (tint.hasMatch()) {
        if (tint.captured(2).toInt() > 255) return false;
        QColor color(tint.captured(1));
        color.setAlpha(tint.captured(2).toInt());
        text.replace(tint.capturedStart(), tint.capturedLength(), color.name(QColor::HexArgb));
        tint = alpha.match(text);
      }
      return true;
    }
    const auto it = values.constFind(match.captured(1));
    if (it == values.cend()) return false;
    // За один проход раскрываем все ссылки: длина цепочки ограничена числом значений.
    QString next;
    qsizetype end = 0;
    auto matches = reference.globalMatch(text);
    while (matches.hasNext()) {
      const auto item = matches.next();
      const auto value = values.constFind(item.captured(1));
      if (value == values.cend()) return false;
      next += text.mid(end, item.capturedStart() - end) + value.value();
      end = item.capturedEnd();
    }
    text = next + text.mid(end);
  }
  return false;
}

}

AppearanceManager::AppearanceManager()
{
  // Значения нужны и до создания главного окна, в том числе при экспорте из CLI.
  applyAvionixTheme();
}

AppearanceManager *AppearanceManager::instance() {
  static AppearanceManager s_instance;
  return &s_instance;
}

bool AppearanceManager::loadStyleSheet(const QString &filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return false;
  }

  QString styleSheet = QString::fromUtf8(file.readAll());
  file.close();

  QHash<QString, QString> values;
  static const QRegularExpression definition(QStringLiteral(R"(@define\s+([\w.]+)\s*=\s*([^;\r\n]+);)"));
  static const QRegularExpression comment(QStringLiteral(R"(/\*[\s\S]*?\*/)"));
  auto comments = comment.globalMatch(styleSheet);
  while (comments.hasNext()) {
    auto definitions = definition.globalMatch(comments.next().captured());
    while (definitions.hasNext()) {
      const auto match = definitions.next();
      if (values.contains(match.captured(1))) return false;
      values.insert(match.captured(1), match.captured(2).trimmed());
    }
  }
  const auto rawValues = values;
  for (auto it = values.begin(); it != values.end(); ++it)
    if (!expandValues(it.value(), rawValues)) return false;

  // Служебные SVG задаются прямо в теме и используют ее палитру.
  // QSS ожидает URL изображения, поэтому держим файлы до следующей смены темы.
  auto styleValues = values;
  QList<QSharedPointer<QTemporaryFile>> images;
  for (auto it = styleValues.begin(); it != styleValues.end(); ++it) {
    if (!it.value().startsWith("<svg ")) continue;
    auto image = QSharedPointer<QTemporaryFile>::create(QDir::tempPath() + "/avionix-theme-XXXXXX.svg");
    const QByteArray data = it.value().toUtf8();
    if (!image->open() || image->write(data) != data.size() || !image->flush()) return false;
    image->close();
    it.value() = QStringLiteral("url(\"%1\")").arg(QDir::fromNativeSeparators(image->fileName()));
    images.append(image);
  }
  styleSheet.remove(comment);
  if (!expandValues(styleSheet, styleValues)) return false;

  m_values = std::move(values);
  if (auto *app = qobject_cast<QApplication *>(QCoreApplication::instance()))
    app->setStyleSheet(styleSheet);
  m_themeImages = std::move(images);
  emit styleChanged();
  return true;
}

void AppearanceManager::applyAvionixTheme() {
  // загружаем тему Avionix Designer из ресурсов
  loadStyleSheet(":/themes/themes/AvionixDesignerTheme.qss");
}

QString AppearanceManager::value(const QString &name) const
{
  const auto it = m_values.constFind(name);
  if (it == m_values.cend()) qWarning() << "Unknown theme value:" << name;
  return it == m_values.cend() ? QString() : it.value();
}

QColor AppearanceManager::color(const QString &name) const
{
  const QColor result(value(name));
  if (!result.isValid()) qWarning() << "Invalid theme color:" << name;
  return result;
}

double AppearanceManager::number(const QString &name) const
{
  bool ok = false;
  const double result = value(name).toDouble(&ok);
  if (!ok || !std::isfinite(result)) {
    qWarning() << "Invalid theme number:" << name;
    return 0;
  }
  return result;
}

QFont AppearanceManager::font(const QString &name, const QFont &base, double scale) const
{
  QFont result(base);
  QStringList families;
  for (auto family : value("font.family").split(','))
    families.append(family.trimmed().remove('"'));
  result.setFamilies(families);
  result.setPixelSize(qMax(1, qRound(number(name + ".size") * scale)));
  result.setWeight(static_cast<QFont::Weight>(qRound(number(name + ".weight"))));
  return result;
}

QPen AppearanceManager::pen(const QString &name, const QColor &color, double scale) const
{
  const auto parts = value(name).simplified().split(' ');
  static const QHash<QString, Qt::PenStyle> styles = {
    {"solid", Qt::SolidLine}, {"dash", Qt::DashLine}, {"dot", Qt::DotLine},
    {"dashDot", Qt::DashDotLine}, {"dashDotDot", Qt::DashDotDotLine}
  };
  bool ok = false;
  const double width = parts.value(0).toDouble(&ok);
  if (!ok || !std::isfinite(width) || width <= 0 || !styles.contains(parts.value(1)))
    qWarning() << "Invalid theme pen:" << name;
  QPen result(color);
  result.setWidthF(width * scale);
  result.setStyle(styles.value(parts.value(1)));
  if (parts.value(2) == "round") result.setCapStyle(Qt::RoundCap);
  return result;
}

QIcon AppearanceManager::colorSwatch(const QString &name, const QColor &color) const
{
  QPixmap pixmap(qRound(number(name + ".width")), qRound(number(name + ".height")));
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setBrush(color);
  painter.setPen(pen("pen.thin", this->color("swatch.border")));
  const double inset = number(name + ".inset"), radius = number(name + ".radius");
  painter.drawRoundedRect(QRectF(pixmap.rect()).adjusted(inset, inset, -inset, -inset), radius, radius);
  return QIcon(pixmap);
}
