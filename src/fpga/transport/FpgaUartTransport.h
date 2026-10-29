// FpgaUartTransport — постоянный UART-сеанс с загрузкой сцены и последними координатами.
#pragma once

#include "fpga/packets/FpgaPacket.h"
#include <QMutex>
#include <QThread>
#include <QWaitCondition>

struct FpgaUartConfig
{
    QString portName;
    qint32 loadBaudRate = 921600;
    qint32 baudRate = 115200;
    int intervalMs = 20; // между началами пакетов; ноль отключает дополнительную паузу
    bool writeInitialZero = true;
    int writeTimeoutMs = 5000;
};

// Порт принадлежит рабочему потоку на весь сеанс. В очереди остаются только
// одна ожидающая сцена и последнее положение; устаревшие положения не накапливаются.
class FpgaUartTransport : public QThread
{
    Q_OBJECT
public:
    explicit FpgaUartTransport(QObject *parent = nullptr);
    ~FpgaUartTransport() override;
    bool startStreaming(const FpgaPacketBundle &bundle, const FpgaUartConfig &config, QString *error = nullptr);
    void stopStreaming();
    void updateScene(const FpgaPacketBundle &bundle);
    void updatePosition(const QByteArray &packet);
    static QByteArray sceneBytes(const FpgaPacketBundle &bundle);

signals:
    void positionRequested();
    void failed(const QString &message);
    void setupComplete();

protected:
    void run() override;

private:
    bool stopping();
    QMutex m_mutex;
    QWaitCondition m_changed;
    FpgaUartConfig m_config;
    QByteArray m_scene;
    QByteArray m_lastScene;
    QByteArray m_lastOrdinary;
    bool m_pendingFullScene = false;
    QByteArray m_position;
    bool m_stop = false;
};
