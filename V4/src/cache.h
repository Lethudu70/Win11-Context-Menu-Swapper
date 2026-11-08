#pragma once
#include <QString>
#include <QStringList>
namespace cms {
class Registry;
struct CleanupResult {
    int removed = 0, archived = 0, preserved = 0;
    QStringList errors;
    bool ok() const { return errors.isEmpty(); }
};
CleanupResult cleanJsonCache(const QString &directory, const Registry &userRegistry, bool resetUndo = false);
}
