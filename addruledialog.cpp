#include "addruledialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QDebug>

AddRuleDialog::AddRuleDialog(QWidget *parent)
    : QDialog(parent) {
    setWindowTitle("new autotrigger");
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    m_vidPidInput = new QLineEdit(this);
    m_vidPidInput->setPlaceholderText("VID:PID");
    
    m_scriptPathInput = new QLineEdit(this);
    m_scriptPathInput->setPlaceholderText("Ścieżka do skryptu");

    m_argsInput = new QLineEdit(this);
    m_argsInput->setPlaceholderText("Argumenty (oddzielone spacją)");
    
    m_authCheckBox = new QCheckBox("Wymaga autoryzacji", this);
    
    m_delaySpinBox = new QSpinBox(this);
    m_delaySpinBox->setSuffix("s");
    m_delaySpinBox->setMinimum(0);
    
    QPushButton *browseButton = new QPushButton("Przeglądaj...", this);
    connect(browseButton, &QPushButton::clicked, this, [this](){
        QString filePath = QFileDialog::getOpenFileName(this, "Wybierz skrypt", QDir::homePath());
        if (!filePath.isEmpty()) {
            m_scriptPathInput->setText(filePath);
        }
    });

    QHBoxLayout *scriptLayout = new QHBoxLayout;
    scriptLayout->addWidget(m_scriptPathInput);
    scriptLayout->addWidget(browseButton);
    
    QPushButton *saveButton = new QPushButton("Dodaj", this);
    QPushButton *cancelButton = new QPushButton("Anuluj", this);
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(saveButton);
    buttonLayout->addWidget(cancelButton);

    connect(saveButton, &QPushButton::clicked, this, &AddRuleDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &AddRuleDialog::reject);

    mainLayout->addWidget(new QLabel("VID:PID:", this));
    mainLayout->addWidget(m_vidPidInput);
    mainLayout->addWidget(new QLabel("Ścieżka do skryptu:", this));
    mainLayout->addLayout(scriptLayout);
    mainLayout->addWidget(new QLabel("Argumenty:", this));
    mainLayout->addWidget(m_argsInput);
    mainLayout->addWidget(new QLabel("Opóźnienie:", this));
    mainLayout->addWidget(m_delaySpinBox);
    mainLayout->addWidget(m_authCheckBox);
    mainLayout->addLayout(buttonLayout);
}

QString AddRuleDialog::getVidPid() const {
    return m_vidPidInput->text();
}

TriggerRule AddRuleDialog::getTriggerRule() const {
    TriggerRule rule;
    rule.script = m_scriptPathInput->text();
    rule.args = m_argsInput->text().split(" ", Qt::SkipEmptyParts);
    rule.authRequired = m_authCheckBox->isChecked();
    rule.delaySec = m_delaySpinBox->value();
    return rule;
}
