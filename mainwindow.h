#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QMenuBar>
#include <QDockWidget>
#include <QTableView>
#include <QSplitter>
#include <QTextEdit>
#include <QAction>
#include "usbmonitor.h"
#include "triggermodel.h"
#include "addruledialog.h"
#include <nlohmann/json.hpp>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onOpenAddTriggerDialog();
    void onRemoveTriggerClicked();
    void onStartMonitoringClicked();
    void onStopMonitoringClicked();
    void onSaveConfigClicked();
    void onOpenConfigClicked();
    void onCheckExistingDevicesClicked();
    void updateLog(const QString& message);
    void updateStatusBar(const QString& message);

private:
    void setupUi();
    void createMenus();

    TriggerModel *m_triggerModel;
    QTableView *m_tableView;
    UsbMonitor *m_usbMonitor;

    QDockWidget *m_autotriggerDock;
    QDockWidget *m_controlsDock;
    QDockWidget *m_logDock;

    QPushButton *m_startMonitorButton;
    QPushButton *m_stopMonitorButton;

    QPlainTextEdit *m_logOutput;
    QStatusBar *m_statusBar;

    QString m_configPath;
};

#endif // MAINWINDOW_H
