#ifndef USBMONITOR_H
#define USBMONITOR_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <libudev.h>
#include <nlohmann/json.hpp>

class UsbMonitor : public QThread {
    Q_OBJECT
public:
    explicit UsbMonitor(QObject* parent = nullptr);
    ~UsbMonitor() override;
    void stop();
    void resetStopFlag();
    void setConfigPath(const QString& path);
    void checkExistingDevices();

signals:
    void logMessage(const QString& message);
    void started();
    void finished();

protected:
    void run() override;

private:
    QMutex m_mutex;
    QWaitCondition m_waitCondition;
    bool m_stop;
    QString m_configPath;

    void processDevice(struct udev_device* dev);
    void executeScript(const QString& script, const QStringList& args, int delay);
    nlohmann::json loadTriggers() const;
};

#endif // USBMONITOR_H
