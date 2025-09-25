#ifndef ADDRULEDIALOG_H
#define ADDRULEDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QStringList>

#include "triggermodel.h"

class AddRuleDialog : public QDialog {
    Q_OBJECT
public:
    explicit AddRuleDialog(QWidget *parent = nullptr);

    QString getVidPid() const;
    TriggerRule getTriggerRule() const;

private:
    QLineEdit *m_vidPidInput;
    QLineEdit *m_scriptPathInput;
    QLineEdit *m_argsInput;
    QCheckBox *m_authCheckBox;
    QSpinBox *m_delaySpinBox;
};

#endif // ADDRULEDIALOG_H
