#include "cache.h"
#include "registry.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>

namespace cms {
CleanupResult cleanJsonCache(const QString &directory, const Registry &userRegistry, bool resetUndo) {
    CleanupResult result;
    const QDir dir(directory);
    const QFileInfo root(directory);
    if (userRegistry.machine() || root.isSymLink() || !root.isDir() ||
        QFileInfo(userRegistry.backupPath()).absoluteFilePath() != QFileInfo(dir.filePath("last-change.json")).absoluteFilePath()) {
        result.errors << "Cleanup is limited to this application's HKCU data directory."; return result;
    }
    const QStringList cacheNames{"cache.json", "state-cache.json", "session-cache.json", "diagnostic-cache.json"};
    const auto files = dir.entryInfoList({"*.json"}, QDir::Files | QDir::Hidden | QDir::System, QDir::Name);
    for (const auto &file : files) {
        if (file.isSymLink()) { ++result.preserved; continue; }
        if (cacheNames.contains(file.fileName())) {
            if (QFile::remove(file.absoluteFilePath())) ++result.removed;
            else result.errors << "Cannot remove " + file.fileName();
        } else if (file.fileName() == "last-change.json") {
            const auto record = userRegistry.backup();
            const bool finished = !record.isEmpty() && record.value("status") == "undone";
            if (finished) {
                if (QFile::remove(file.absoluteFilePath())) ++result.removed;
                else result.errors << "Cannot remove completed undo history";
            } else if (record.isEmpty() || resetUndo) {
                const auto archive = dir.filePath("archive");
                if (QFileInfo(archive).isSymLink() || !QDir().mkpath(archive)) {
                    result.errors << "Cannot create archive directory"; continue;
                }
                const auto name = "last-change-" + QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz") + "-" +
                    QUuid::createUuid().toString(QUuid::WithoutBraces) + ".json.bak";
                if (QFile::rename(file.absoluteFilePath(), QDir(archive).filePath(name))) ++result.archived;
                else result.errors << "Cannot archive undo history";
            } else ++result.preserved;
        } else ++result.preserved; // Unknown configuration files are never treated as cache.
    }
    return result;
}
}
