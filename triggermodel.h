#ifndef TRIGGERMODEL_H
#define TRIGGERMODEL_H

#include <QAbstractTableModel>
#include <QFile>
#include <QList>
#include <QVariant>
#include <QStringList>
#include <QDebug>
#include <QMap>

struct TriggerRule {
    QString vidPid;
    QString script;
    QStringList args;
    bool authRequired;
    int delaySec;
};

class TriggerModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit TriggerModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void addTrigger(const QString& vidPid, const TriggerRule& rule);
    void removeTriggers(const QString& vidPid);
    bool loadTriggers(const QString& filePath);
    bool saveTriggers(const QString& filePath);
    
private:
    QList<TriggerRule> m_triggers;
};

#endif // TRIGGERMODEL_H
