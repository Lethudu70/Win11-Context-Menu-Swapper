#pragma once
#include <QJsonObject>
#include <QString>

namespace cms {
struct Result {
    bool ok = false;
    QString message;
    unsigned long windowsError = 0;
};
struct State {
    bool ok = false;
    QJsonObject tree;
    QString error;
    QJsonObject secondary;
    QString mode() const;
};
enum class Hive { CurrentUser, LocalMachine };
// One explicit hive and subtree. Tests can redirect HKLM to an isolated HKCU key.
class Registry {
public:
    Registry(QString path, QString backupPath, Hive hive = Hive::CurrentUser, bool allowProtectedWrite = false,
             QString secondaryPath = {});
    State read() const;
    Result apply(bool classic);
    Result undo();
    bool canUndo() const;
    QString backupPath() const;
    QString path() const { return path_; }
    QString targets() const;
    QString hiveName() const { return hive_ == Hive::LocalMachine ? "HKLM" : "HKCU"; }
    bool machine() const { return hive_ == Hive::LocalMachine; }
    QJsonObject backup() const;
private:
    QString path_, backup_, secondaryPath_;
    Hive hive_;
    bool allowProtectedWrite_;
    void *root() const;
    State readSingle(const QString &path) const;
    Result restore(const QString &path, const QJsonObject &tree);
    Result setClassic(const QString &path, const QJsonObject &before);
    Result writeBackup(const QJsonObject &record) const;
};
QString systemDescription();
QString productionPath();
QString secondaryProductionPath();
QString machineBackupKey();
int selfTest(const QString &reportPath);
}
