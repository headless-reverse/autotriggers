#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QTableView>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QSplitter>
#include <QTextEdit>
#include <QFileDialog>
#include <QSettings>
#include <QModelIndex>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_usbMonitor(new UsbMonitor(this)) {

    m_configPath = QDir::currentPath() + "/triggers.json";
    m_triggerModel = new TriggerModel(this);
    m_triggerModel->loadTriggers(m_configPath);

    setupUi();
    createMenus();

    connect(m_usbMonitor, &UsbMonitor::logMessage, this, &MainWindow::updateLog);
    connect(m_usbMonitor, &UsbMonitor::logMessage, this, &MainWindow::updateStatusBar);
    
    connect(m_usbMonitor, &UsbMonitor::started, this, [this](){
        m_startMonitorButton->setEnabled(false);
        m_stopMonitorButton->setEnabled(true);
        updateStatusBar("udev_monitor active.");
    });
    connect(m_usbMonitor, &UsbMonitor::finished, this, [this](){
        m_startMonitorButton->setEnabled(true);
        m_stopMonitorButton->setEnabled(false);
        updateStatusBar("udev_monitor stopped.");
    });

    updateStatusBar("Gotowy.");
}

MainWindow::~MainWindow() {
    if (m_usbMonitor->isRunning()) {
        m_usbMonitor->stop();
        m_usbMonitor->wait();
    }
    delete m_triggerModel;
}

void MainWindow::setupUi() {
    setWindowTitle("autotriggers");

    m_controlsDock = new QDockWidget("Control", this);
    m_controlsDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    
    QWidget *controlsWidget = new QWidget(this);
    QHBoxLayout *controlLayout = new QHBoxLayout(controlsWidget);
    m_startMonitorButton = new QPushButton("Start", this);
    m_stopMonitorButton = new QPushButton("Stop", this);
    m_stopMonitorButton->setEnabled(false);
    QPushButton *checkExistingButton = new QPushButton("Sprawdź istniejące", this);
    controlLayout->addWidget(m_startMonitorButton);
    controlLayout->addWidget(m_stopMonitorButton);
    controlLayout->addWidget(checkExistingButton);
    m_controlsDock->setWidget(controlsWidget);

    m_autotriggerDock = new QDockWidget("autotrigger", this);
    m_autotriggerDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    
    QWidget *tableWidget = new QWidget(this);
    QVBoxLayout *tableLayout = new QVBoxLayout(tableWidget);
    m_tableView = new QTableView(this);
    m_tableView->setModel(m_triggerModel);
    m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    QPushButton *removeButton = new QPushButton("Usuń autotrigger", this);
    tableLayout->addWidget(m_tableView);
    tableLayout->addWidget(removeButton);
    m_autotriggerDock->setWidget(tableWidget);

    m_logDock = new QDockWidget(tr("Log output"), this);
    m_logDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    m_logOutput = new QPlainTextEdit(this);
    m_logOutput->setReadOnly(true);
    m_logDock->setWidget(m_logOutput);

    addDockWidget(Qt::RightDockWidgetArea, m_controlsDock);
    splitDockWidget(m_controlsDock, m_autotriggerDock, Qt::Vertical);
    splitDockWidget(m_autotriggerDock, m_logDock, Qt::Vertical);
    
    setCentralWidget(nullptr);

    m_statusBar = new QStatusBar(this);
    setStatusBar(m_statusBar);

    connect(removeButton, &QPushButton::clicked, this, &MainWindow::onRemoveTriggerClicked);
    connect(m_startMonitorButton, &QPushButton::clicked, this, &MainWindow::onStartMonitoringClicked);
    connect(m_stopMonitorButton, &QPushButton::clicked, this, &MainWindow::onStopMonitoringClicked);
    connect(checkExistingButton, &QPushButton::clicked, this, &MainWindow::onCheckExistingDevicesClicked);
}

void MainWindow::createMenus() {
    QMenu *fileMenu = menuBar()->addMenu(tr("&Plik"));
    fileMenu->addAction(tr("Dodaj autotrigger..."), this, &MainWindow::onOpenAddTriggerDialog);
    fileMenu->addAction(tr("Sprawdź istniejące..."), this, &MainWindow::onCheckExistingDevicesClicked);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("Otwórz konfigurację..."), this, &MainWindow::onOpenConfigClicked);
    fileMenu->addAction(tr("Zapisz konfigurację..."), this, &MainWindow::onSaveConfigClicked);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("Wyjście"), this, &MainWindow::close);

    QMenu *viewMenu = menuBar()->addMenu(tr("&Widok"));
    viewMenu->addAction(m_autotriggerDock->toggleViewAction());
    viewMenu->addAction(m_controlsDock->toggleViewAction());
    viewMenu->addAction(m_logDock->toggleViewAction());

    QMenu *helpMenu = menuBar()->addMenu(tr("&Pomoc"));
    helpMenu->addAction(tr("program..."), this, [](){
        QMessageBox::about(nullptr, "autotrigger", "autotrigger _by hazeDev\nAplikacja wykonuje autoakcje po VID:PID.");
    });
}

void MainWindow::onOpenAddTriggerDialog() {
    AddRuleDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        TriggerRule rule = dialog.getTriggerRule();
        QString vidPid = dialog.getVidPid();
        if (!vidPid.isEmpty() && !rule.script.isEmpty()) {
            m_triggerModel->addTrigger(vidPid, rule);
            updateLog(QString("[✓] Dodano autotrigger dla VID:PID %1").arg(vidPid));
        } else {
            QMessageBox::warning(this, "Błąd", "Nie można dodać autotriggera. Wprowadź VID:PID i ścieżkę do skryptu.");
        }
    }
}

void MainWindow::onRemoveTriggerClicked() {
    QModelIndexList selection = m_tableView->selectionModel()->selectedRows();
    if (selection.isEmpty()) {
        return;
    }
    
    int row = selection.first().row();
    QString vidPid = m_triggerModel->data(m_triggerModel->index(row, 0)).toString();
    m_triggerModel->removeTriggers(vidPid);

    updateLog(QString("[✓] Usunięto autotrigger dla VID:PID %1").arg(vidPid));
}

void MainWindow::onStartMonitoringClicked() {
    m_usbMonitor->setConfigPath(m_configPath);
    m_usbMonitor->resetStopFlag();
    m_usbMonitor->start();
}

void MainWindow::onStopMonitoringClicked() {
    m_usbMonitor->stop();
}

void MainWindow::onCheckExistingDevicesClicked() {
    m_usbMonitor->setConfigPath(m_configPath);
    m_usbMonitor->checkExistingDevices();
}

void MainWindow::onSaveConfigClicked() {
    QString fileName = QFileDialog::getSaveFileName(this, tr("Zapisz konfigurację"), m_configPath, tr("Pliki JSON (*.json)"));
    if (fileName.isEmpty()) {
        return;
    }
    m_triggerModel->saveTriggers(fileName);
    m_configPath = fileName;
    updateStatusBar("Konfiguracja zapisana.");
}

void MainWindow::onOpenConfigClicked() {
    QString fileName = QFileDialog::getOpenFileName(this, tr("Otwórz konfigurację"), m_configPath, tr("Pliki JSON (*.json)"));
    if (fileName.isEmpty()) {
        return;
    }
    m_triggerModel->loadTriggers(fileName);
    m_configPath = fileName;
    updateStatusBar("Konfiguracja wczytana.");
}

void MainWindow::updateLog(const QString& message) {
    m_logOutput->appendPlainText(message);
}

void MainWindow::updateStatusBar(const QString& message) {
    m_statusBar->showMessage(message);
}
