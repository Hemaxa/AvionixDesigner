#include "AppearanceManager.h"
#include <QApplication>
#include <QFile>

AppearanceManager::AppearanceManager() = default;

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

  qApp->setStyleSheet(styleSheet);
  m_currentStylePath = filePath;

  emit styleChanged();
  return true;
}

void AppearanceManager::applyAvionixTheme() {
  // загружаем тему Avionix Designer из ресурсов
  loadStyleSheet(":/themes/themes/AvionixDesignerTheme.qss");
}

QString AppearanceManager::getCurrentStylePath() const {
  return m_currentStylePath;
}
