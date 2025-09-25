#include "usbmonitor.h"
#include <QDebug>
#include <QProcess>
#include <QThread>
#include <fstream>
#include <QDir>
#include <iostream>

UsbMonitor::UsbMonitor(QObject* parent)
    : QThread(parent), m_stop(false) {}

UsbMonitor::~UsbMonitor() {
    stop();
    wait();
}

void UsbMonitor::stop() {
    m_stop = true;
}

void UsbMonitor::resetStopFlag() {
    m_stop = false;
}

void UsbMonitor::setConfigPath(const QString& path) {
    m_configPath = path;
}

void UsbMonitor::checkExistingDevices() {
    emit logMessage("[•] Skanowanie istniejących urządzeń USB...");
    struct udev* udev = udev_new();
    if (!udev) {
        emit logMessage("[!] Nie można utworzyć kontekstu udev do skanowania.");
        return;
    }

    struct udev_enumerate* enumerate = udev_enumerate_new(udev);
    if (!enumerate) {
        emit logMessage("[!] Nie można utworzyć enumeratora udev.");
        udev_unref(udev);
        return;
    }

    udev_enumerate_add_match_subsystem(enumerate, "usb");
    udev_enumerate_scan_devices(enumerate);

    struct udev_list_entry* devices;
    struct udev_list_entry* entry;

    devices = udev_enumerate_get_list_entry(enumerate);
    udev_list_entry_foreach(entry, devices) {
        const char* path = udev_list_entry_get_name(entry);
        struct udev_device* dev = udev_device_new_from_syspath(udev, path);
        if (dev) {
            processDevice(dev);
            udev_device_unref(dev);
        }
    }

    udev_enumerate_unref(enumerate);
    udev_unref(udev);
    emit logMessage("[✓] Zakończono skanowanie istniejących urządzeń.");
}

void UsbMonitor::processDevice(struct udev_device* dev) {
    const char* action = udev_device_get_action(dev);
    if (udev_device_get_devtype(dev) && QString(udev_device_get_devtype(dev)) == "usb_device") {
        const char* vendorId = udev_device_get_sysattr_value(dev, "idVendor");
        const char* productId = udev_device_get_sysattr_value(dev, "idProduct");
        
        if (vendorId && productId) {
            QString vidPid = QString("%1:%2").arg(vendorId).arg(productId);
            const char* product = udev_device_get_sysattr_value(dev, "product");
            const char* manufacturer = udev_device_get_sysattr_value(dev, "manufacturer");
            QString deviceName = (manufacturer && product) ?
                                 QString("%1 %2").arg(manufacturer).arg(product) :
                                 "nieznane urządzenie";
                                 
            emit logMessage(QString("[•] Sprawdzanie: %1 (%2)").arg(deviceName).arg(vidPid));

            nlohmann::json triggers = loadTriggers();

            if (triggers.contains(vidPid.toStdString())) {
                auto rulesArray = triggers[vidPid.toStdString()];
                if (rulesArray.is_array()) {
                    emit logMessage(QString("  [•] Znaleziono %1 akcji dla VID:PID %2.").arg(rulesArray.size()).arg(vidPid));
                    for (const auto& ruleObj : rulesArray) {
                        QString script = QString::fromStdString(ruleObj.at("action_script").get<std::string>());
                        int delay = ruleObj.at("delay_sec").get<int>();
                        
                        QStringList args;
                        if (ruleObj.contains("action_args") && ruleObj.at("action_args").is_array()) {
                            for (const auto& argValue : ruleObj.at("action_args")) {
                                args.append(QString::fromStdString(argValue.get<std::string>()));
                            }
                        }
                        executeScript(script, args, delay);
                    }
                }
            }
        }
    }
}


void UsbMonitor::executeScript(const QString& script, const QStringList& args, int delay) {
    if (delay > 0) {
        emit logMessage(QString("[•] Opóźnienie %1s dla '%2'").arg(delay).arg(script));
        QThread::sleep(delay);
    }

    QProcess *process = new QProcess(this);
    process->setProgram(script);
    process->setArguments(args);
    process->startDetached();

    emit logMessage(QString("[✓] Akcja '%1' uruchomiona.").arg(script));
}

nlohmann::json UsbMonitor::loadTriggers() const {
    nlohmann::json triggers;
    std::ifstream file(m_configPath.toStdString());
    if (!file.is_open()) {
        return triggers;
    }
    try {
        file >> triggers;
    } catch (nlohmann::json::parse_error& e) {
        qWarning() << "Błąd parsowania JSON:" << e.what();
    }
    return triggers;
}

void UsbMonitor::run() {
    emit started();
    emit logMessage("[•] Uruchomiono monitoring zdarzeń USB.");
    
    struct udev* udev = udev_new();
    if (!udev) {
        emit logMessage("[!] Nie można utworzyć kontekstu udev.");
        emit finished();
        return;
    }

    struct udev_monitor* mon = udev_monitor_new_from_netlink(udev, "udev");
    if (!mon) {
        emit logMessage("[!] Nie można utworzyć monitora udev.");
        udev_unref(udev);
        emit finished();
        return;
    }

    udev_monitor_filter_add_match_subsystem_devtype(mon, "usb", "usb_device");
    udev_monitor_enable_receiving(mon);
    int fd = udev_monitor_get_fd(mon);

    while (!m_stop) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        struct timeval tv = {0, 100000};
        select(fd + 1, &fds, NULL, NULL, &tv);

        if (FD_ISSET(fd, &fds)) {
            struct udev_device* dev = udev_monitor_receive_device(mon);
            if (dev) {
                const char* action = udev_device_get_action(dev);
                if (action && QString(action) == "add") {
                    processDevice(dev);
                }
                udev_device_unref(dev);
            }
        }
    }
    udev_monitor_unref(mon);
    udev_unref(udev);
    emit logMessage("[✓] Zatrzymano monitoring.");
    emit finished();
}
