#include "fpga/transport/FpgaUartTransport.h"
#include <QElapsedTimer>
#include <QMutexLocker>
#include <QSerialPort>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#else
#include <cerrno>
#include <termios.h>
#endif

FpgaUartTransport::FpgaUartTransport(QObject *parent) : QThread(parent) {}
FpgaUartTransport::~FpgaUartTransport() { stopStreaming(); }

QByteArray FpgaUartTransport::sceneBytes(const FpgaPacketBundle &bundle)
{
    QByteArray bytes;
    for (const auto &packet : bundle.packets) {
        if (packet.type != FpgaPacketType::Layer3d || packet.payloadSize == 1)
            bytes.append(packet.bytes);
    }
    return bytes;
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
        m_lastScene = m_scene = sceneBytes(bundle);
        m_lastOrdinary.clear();
        for (const auto &packet : bundle.packets)
            if (packet.type != FpgaPacketType::Layer3d) m_lastOrdinary.append(packet.bytes);
        m_pendingFullScene = true;
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
    const QByteArray bytes = sceneBytes(bundle);
    QByteArray ordinary, side;
    for (const auto &packet : bundle.packets) {
        if (packet.type != FpgaPacketType::Layer3d) ordinary.append(packet.bytes);
        else if (packet.payloadSize == 1) side = packet.bytes;
    }
    const QMutexLocker lock(&m_mutex);
    if (bytes == m_lastScene) return;
    m_pendingFullScene = m_pendingFullScene || ordinary != m_lastOrdinary;
    m_scene = m_pendingFullScene ? bytes : side;
    m_lastScene = bytes;
    m_lastOrdinary = ordinary;
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
    QByteArray initialScene;
    {
        const QMutexLocker lock(&m_mutex);
        config = m_config;
        initialScene = m_scene;
        m_scene.clear();
        m_pendingFullScene = false;
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
            // Drain the driver too, especially BEFORE switching baud rate.
            // This can block, hence the port and all I/O live off the GUI thread.
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
        bool isPosition = false;
        {
            QMutexLocker lock(&m_mutex);
            while (!m_stop) {
                if (!m_scene.isEmpty()) { bytes.swap(m_scene); m_pendingFullScene = false; break; }
                const qint64 waitMs = nextPosition - clock.elapsed();
                if (!m_position.isEmpty() && waitMs <= 0) {
                    bytes.swap(m_position); isPosition = true; break;
                }
                if (!m_position.isEmpty() && waitMs > 0) m_changed.wait(&m_mutex, static_cast<unsigned long>(waitMs));
                else m_changed.wait(&m_mutex);
            }
            if (m_stop) break;
        }
        if (isPosition) nextPosition = clock.elapsed() + config.intervalMs;
        success = write(bytes);
    }
    port.close();
    if (!success && !stopping()) emit failed(QStringLiteral("UART %1: %2").arg(config.portName, error));
}
