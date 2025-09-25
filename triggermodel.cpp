#include "triggermodel.h"
#include <QMessageBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>
#include <QFile>
#include <QMap>

TriggerModel::TriggerModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int TriggerModel::rowCount(const QModelIndex& parent) const {
    Q_UNUSED(parent);
    return m_triggers.size();
}

int TriggerModel::columnCount(const QModelIndex& parent) const {
    Q_UNUSED(parent);
    return 5;
}

QVariant TriggerModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_triggers.size()) {
        return QVariant();
    }
    
    if (role == Qt::DisplayRole) {
        const TriggerRule& rule = m_triggers.at(index.row());
        switch (index.column()) {
            case 0: return rule.vidPid;
            case 1: return rule.script;
            case 2: return rule.args.join(" ");
            case 3: return rule.authRequired ? "Tak" : "Nie";
            case 4: return rule.delaySec;
        }
    }
    return QVariant();
}

QVariant TriggerModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
            case 0: return "VID:PID";
            case 1: return "Skrypt";
            case 2: return "Argumenty";
            case 3: return "Auth";
            case 4: return "Opóźnienie";
        }
    }
    return QVariant();
}

void TriggerModel::addTrigger(const QString& vidPid, const TriggerRule& rule) {
    beginInsertRows(QModelIndex(), m_triggers.size(), m_triggers.size());
    m_triggers.append(rule);
    m_triggers.last().vidPid = vidPid;
    endInsertRows();
}

void TriggerModel::removeTriggers(const QString& vidPid) {
    QList<int> rowsToRemove;
    for (int i = 0; i < m_triggers.size(); ++i) {
        if (m_triggers.at(i).vidPid == vidPid) {
            rowsToRemove.prepend(i);
        }
    }
    
    for (int row : rowsToRemove) {
        beginRemoveRows(QModelIndex(), row, row);
        m_triggers.removeAt(row);
        endRemoveRows();
    }
}

bool TriggerModel::loadTriggers(const QString& filePath) {
    beginResetModel();
    m_triggers.clear();
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        endResetModel();
        return false;
    }

    QByteArray file_data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(file_data);
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "failed to create json document or not an object";
        endResetModel();
        return false;
    }

    QJsonObject obj = doc.object();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        QString vidPid = it.key();
        QJsonArray rules_array = it.value().toArray();

        for (const QJsonValue& value : rules_array) {
            QJsonObject rule_obj = value.toObject();
            TriggerRule rule;
            rule.vidPid = vidPid;
            rule.script = rule_obj["action_script"].toString();
            rule.authRequired = rule_obj["auth_required"].toBool();
            rule.delaySec = rule_obj["delay_sec"].toInt();
            
            QJsonArray args_array = rule_obj["action_args"].toArray();
            for (const QJsonValue& arg_value : args_array) {
                rule.args.append(arg_value.toString());
            }
            m_triggers.append(rule);
        }
    }
    endResetModel();
    return true;
}

bool TriggerModel::saveTriggers(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QJsonObject main_obj;
    for (const TriggerRule& rule : m_triggers) {
        QJsonArray rules_array;
        if (main_obj.contains(rule.vidPid)) {
            rules_array = main_obj[rule.vidPid].toArray();
        }
        
        QJsonObject rule_obj;
        rule_obj["action_script"] = rule.script;
        rule_obj["auth_required"] = rule.authRequired;
        rule_obj["delay_sec"] = rule.delaySec;
        
        QJsonArray args_array;
        for (const QString& arg : rule.args) {
            args_array.append(arg);
        }
        rule_obj["action_args"] = args_array;
        rules_array.append(rule_obj);
        main_obj[rule.vidPid] = rules_array;
    }

    QJsonDocument doc(main_obj);
    file.write(doc.toJson());
    file.close();

    return true;
}
