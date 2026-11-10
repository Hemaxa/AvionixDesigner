// FpgaUartTransport - постоянный UART-сеанс с загрузкой сцены и последними координатами.

#include "fpga/transport/FpgaUartTransport.h"
#include <QElapsedTimer>
#include <QMutexLocker>
#include <QSerialPort>
#include <utility>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#else
#include <cerrno>
#include <termios.h>
#endif

namespace {
QList<FpgaPacket> scenePackets(const FpgaPacketBundle &bundle)
{
    auto packets = bundle.packets;
    packets.removeIf([](const FpgaPacket &packet) {
        return packet.type == FpgaPacketType::Layer3d && packet.payloadSize != 1;
    });
    return packets;
}

QByteArray packetBytes(const QList<FpgaPacket> &packets)
{
    QByteArray bytes;
    for (const auto &packet : packets)
        bytes.append(packet.bytes);
    return bytes;
}

QByteArray updateBytes(const QList<FpgaPacket> &sent, const QList<FpgaPacket> &next)
{
    if (sent.size() != next.size())
        return packetBytes(next);
    QByteArray changes;
    for (qsizetype i = 0; i < next.size(); ++i) {
        const auto &packet = next[i];
        const auto &previous = sent[i];
        if (packet.type != previous.type || packet.objId != previous.objId || packet.objectType != previous.objectType)
            return packetBytes(next);
        if (packet.bytes == previous.bytes)
            continue;
        if (packet.type == FpgaPacketType::StaticMemory || packet.type == FpgaPacketType::RotationMemory)
            return packetBytes(next);
        changes.append(packet.bytes);
    }
    return changes;
}
}

FpgaUartTransport::FpgaUartTransport(QObject *parent) : QThread(parent) {}
FpgaUartTransport::~FpgaUartTransport() { stopStreaming(); }

QByteArray FpgaUartTransport::sceneBytes(const FpgaPacketBundle &bundle)
{
    return packetBytes(scenePackets(bundle));
}

bool FpgaUartTransport::startStreaming(const FpgaPacketBundle &bundle, const FpgaUartConfig &config, QString *error)
{
    if (error) error->clear();
    if (isRunning() || config.portName.trimmed().isEmpty() || config.baudRate <= 0
        || config.loadBaudRate <= 0 || config.intervalMs < 0 || config.writeTimeoutMs <= 0) {
        if (error) *error = QStringLiteral("Неверные настройки UART или сеанс уже запущен.");
        return false;
    }
    {
        const QMutexLocker lock(&m_mutex);
        m_config = config;
        m_stop = false;
        m_scene = scenePackets(bundle);
        m_scenePending = true;
        m_position.clear();
    }
    start();
    return true;
}

void FpgaUartTransport::stopStreaming()
{
    {
        const QMutexLocker lock(&m_mutex);
        m_stop = true;
        m_changed.wakeAll();
    }
    if (QThread::currentThread() != this) wait();
}

void FpgaUartTransport::updateScene(const FpgaPacketBundle &bundle)
{
    const QMutexLocker lock(&m_mutex);
    m_scene = scenePackets(bundle);
    m_scenePending = true;
    m_changed.wakeAll();
}

void FpgaUartTransport::updatePosition(const QByteArray &packet)
{
    const QMutexLocker lock(&m_mutex);
    m_position = packet;
    m_changed.wakeAll();
}

bool FpgaUartTransport::stopping()
{
    const QMutexLocker lock(&m_mutex);
    return m_stop;
}

void FpgaUartTransport::run()
{
    FpgaUartConfig config;
    QList<FpgaPacket> sent;
    {
        const QMutexLocker lock(&m_mutex);
        config = m_config;
        sent = std::exchange(m_scene, {});
        m_scenePending = false;
    }
    QSerialPort port;
    port.setPortName(config.portName.trimmed());
    port.setBaudRate(config.loadBaudRate);
    port.setDataBits(QSerialPort::Data8);
    port.setParity(QSerialPort::NoParity);
    port.setStopBits(QSerialPort::OneStop);
    port.setFlowControl(QSerialPort::NoFlowControl);
    QString error;
    if (!port.open(QIODevice::WriteOnly)) {
        emit failed(QStringLiteral("Не удалось открыть UART %1: %2").arg(config.portName, port.errorString()));
        return;
    }
    auto write = [&](const QByteArray &data) {
        qsizetype offset = 0;
        while (offset < data.size() && !stopping()) {
            QElapsedTimer deadline;
            deadline.start();
            const qsizetype chunkSize = qMin(qsizetype(1024), data.size() - offset);
            const qint64 accepted = port.write(data.constData() + offset, chunkSize);
            if (accepted < 0) { error = port.errorString(); return false; }
            offset += accepted;
            while (port.bytesToWrite() > 0) {
                const int remaining = config.writeTimeoutMs - int(deadline.elapsed());
                if (remaining <= 0) { error = QStringLiteral("Истекло время записи UART."); return false; }
                if (!port.waitForBytesWritten(qMin(50, remaining))
                    && port.error() != QSerialPort::TimeoutError && port.error() != QSerialPort::NoError) {
                    error = port.errorString(); return false;
                }
            }
            // Перед сменой скорости нужно дождаться опустошения также и буфера драйвера.
            // Ожидание может блокировать поток, поэтому весь ввод/вывод вынесен из потока интерфейса.
#ifdef Q_OS_WIN
            if (!FlushFileBuffers(reinterpret_cast<HANDLE>(port.handle()))) {
                error = QStringLiteral("Не удалось завершить передачу UART (%1).").arg(GetLastError());
                return false;
            }
#else
            int result;
            do { result = tcdrain(int(port.handle())); } while (result < 0 && errno == EINTR && !stopping());
            if (result < 0) { error = QStringLiteral("Не удалось завершить передачу UART (%1).").arg(errno); return false; }
#endif
            if (accepted == 0) {
                error = QStringLiteral("UART не принимает данные для записи.");
                return false;
            }
        }
        return !stopping();
    };
    QByteArray initialScene = packetBytes(sent);
    if (config.writeInitialZero) initialScene.prepend(char(0));
    bool success = write(initialScene);
    if (success && !port.setBaudRate(config.baudRate)) {
        error = port.errorString(); success = false;
    }
    if (success) emit setupComplete();
    QElapsedTimer clock;
    clock.start();
    qint64 nextPosition = 0;
    while (success && !stopping()) {
        emit positionRequested();
        QByteArray bytes;
        QList<FpgaPacket> nextScene;
        bool isScene = false;
        bool isPosition = false;
        {
            QMutexLocker lock(&m_mutex);
            while (!m_stop) {
                if (m_scenePending) {
                    nextScene.swap(m_scene);
                    m_scenePending = false;
                    isScene = true;
                    break;
                }
                const qint64 waitMs = nextPosition - clock.elapsed();
                if (!m_position.isEmpty() && waitMs <= 0) {
                    bytes.swap(m_position); isPosition = true; break;
                }
                if (!m_position.isEmpty() && waitMs > 0) m_changed.wait(&m_mutex, static_cast<unsigned long>(waitMs));
                else m_changed.wait(&m_mutex);
            }
            if (m_stop) break;
        }
        if (isScene) {
            // Сравниваем с уже отправленной сценой, даже если промежуточные правки были вытеснены.
            bytes = updateBytes(sent, nextScene);
            sent = std::move(nextScene);
        }
        if (isPosition) nextPosition = clock.elapsed() + config.intervalMs;
        success = write(bytes);
    }
    port.close();
    if (!success && !stopping()) emit failed(QStringLiteral("UART %1: %2").arg(config.portName, error));
}
