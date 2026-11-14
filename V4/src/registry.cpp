#include "registry.h"
#include <windows.h>
#include <sddl.h>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <utility>
#include <vector>

namespace cms {
namespace {
QString errorText(LSTATUS code) {
    wchar_t *buffer = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                   FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0,
                   reinterpret_cast<wchar_t *>(&buffer), 0, nullptr);
    QString text = buffer ? QString::fromWCharArray(buffer).trimmed() : QStringLiteral("Windows error");
    if (buffer) LocalFree(buffer);
    return QStringLiteral("%1 (%2)").arg(text).arg(code);
}
struct Key {
    HKEY h = nullptr;
    ~Key() { if (h) RegCloseKey(h); }
};
// Enable only an already assigned Windows privilege, then restore its prior state.
// No ACL or owner is modified. Handles opened during the scope retain their rights.
struct RestorePrivilege {
    HANDLE token = nullptr;
    TOKEN_PRIVILEGES previous{};
    bool adjusted = false;
    DWORD error = ERROR_SUCCESS;
    RestorePrivilege() {
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &token)) { error = GetLastError(); return; }
        TOKEN_PRIVILEGES wanted{}; wanted.PrivilegeCount = 1;
        if (!LookupPrivilegeValueW(nullptr, SE_RESTORE_NAME, &wanted.Privileges[0].Luid)) { error = GetLastError(); return; }
        wanted.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        DWORD size = sizeof(previous); SetLastError(ERROR_SUCCESS);
        const bool ok = AdjustTokenPrivileges(token, FALSE, &wanted, sizeof(previous), &previous, &size);
        error = GetLastError(); adjusted = ok;
        if (!ok && error == ERROR_SUCCESS) error = ERROR_PRIVILEGE_NOT_HELD;
    }
    ~RestorePrivilege() {
        if (token && adjusted) AdjustTokenPrivileges(token, FALSE, &previous, 0, nullptr, nullptr);
        if (token) CloseHandle(token);
    }
};
LSTATUS openProtectedValue(HKEY root, const wchar_t *path, HKEY *key) {
    RestorePrivilege privilege;
    if (privilege.error != ERROR_SUCCESS) return privilege.error;
    return RegCreateKeyExW(root, path, 0, nullptr, REG_OPTION_BACKUP_RESTORE,
        KEY_WOW64_64KEY, nullptr, key, nullptr);
}
bool readTree(HKEY key, QJsonObject &out, QString &error, int depth = 0) {
    if (depth > 32) { error = "Registry subtree too deep"; return false; }
    DWORD values = 0, maxName = 0, maxData = 0, subkeys = 0, maxSub = 0;
    auto rc = RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, &subkeys, &maxSub, nullptr,
                              &values, &maxName, &maxData, nullptr, nullptr);
    if (rc != ERROR_SUCCESS) { error = errorText(rc); return false; }
    if (values > 4096 || subkeys > 4096 || maxData > 8 * 1024 * 1024) {
        error = "Registry subtree exceeds safety limits"; return false;
    }
    QJsonObject entries, children;
    std::vector<wchar_t> name(qMax(maxName, maxSub) + 2);
    QByteArray bytes(static_cast<qsizetype>(maxData) + 2, '\0');
    for (DWORD i = 0; i < values; ++i) {
        DWORD n = static_cast<DWORD>(name.size()), size = static_cast<DWORD>(bytes.size()), type = 0;
        rc = RegEnumValueW(key, i, name.data(), &n, nullptr, &type,
                           reinterpret_cast<BYTE *>(bytes.data()), &size);
        if (rc != ERROR_SUCCESS) { error = errorText(rc); return false; }
        entries.insert(QString::fromWCharArray(name.data(), n), QJsonObject{
            {"type", static_cast<int>(type)}, {"data", QString::fromLatin1(bytes.left(size).toBase64())}});
    }
    for (DWORD i = 0; i < subkeys; ++i) {
        DWORD n = static_cast<DWORD>(name.size());
        rc = RegEnumKeyExW(key, i, name.data(), &n, nullptr, nullptr, nullptr, nullptr);
        if (rc != ERROR_SUCCESS) { error = errorText(rc); return false; }
        Key child;
        rc = RegOpenKeyExW(key, name.data(), 0, KEY_READ, &child.h);
        if (rc != ERROR_SUCCESS) { error = errorText(rc); return false; }
        QJsonObject tree;
        if (!readTree(child.h, tree, error, depth + 1)) return false;
        children.insert(QString::fromWCharArray(name.data(), n), tree);
    }
    out = {{"exists", true}, {"values", entries}, {"children", children}};
    return true;
}
LSTATUS writeTree(HKEY key, const QJsonObject &tree) {
    const auto entries = tree.value("values").toObject();
    for (auto it = entries.begin(); it != entries.end(); ++it) {
        const auto value = it.value().toObject();
        const auto data = QByteArray::fromBase64(value.value("data").toString().toLatin1());
        const auto name = it.key().toStdWString();
        auto rc = RegSetValueExW(key, name.c_str(), 0, value.value("type").toInt(),
             reinterpret_cast<const BYTE *>(data.constData()), static_cast<DWORD>(data.size()));
        if (rc != ERROR_SUCCESS) return rc;
    }
    const auto children = tree.value("children").toObject();
    for (auto it = children.begin(); it != children.end(); ++it) {
        Key child;
        const auto name = it.key().toStdWString();
        auto rc = RegCreateKeyExW(key, name.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &child.h, nullptr);
        if (rc != ERROR_SUCCESS) return rc;
        rc = writeTree(child.h, it.value().toObject());
        if (rc != ERROR_SUCCESS) return rc;
    }
    return ERROR_SUCCESS;
}
bool validTree(const QJsonObject &tree, int depth = 0) {
    if (depth > 32 || !tree.value("exists").isBool()) return false;
    if (!tree.value("exists").toBool()) return tree.size() == 1;
    if (!tree.value("values").isObject() || !tree.value("children").isObject()) return false;
    const auto values = tree.value("values").toObject();
    for (auto it = values.begin(); it != values.end(); ++it) {
        const auto v = it.value().toObject();
        if (it.key().contains(QChar('\0')) || !v.value("type").isDouble() ||
            v.value("type").toInt(-1) < 0 || !v.value("data").isString()) return false;
        const auto b = v.value("data").toString().toLatin1();
        if (QByteArray::fromBase64(b).toBase64() != b) return false;
    }
    const auto children = tree.value("children").toObject();
    for (auto it = children.begin(); it != children.end(); ++it)
        if (it.key().isEmpty() || it.key().contains('\\') || it.key().contains(QChar('\0')) ||
            !validTree(it.value().toObject(), depth + 1)) return false;
    return true;
}
QJsonObject classicTree(QJsonObject tree) {
    tree["exists"] = true;
    if (!tree.contains("values")) tree["values"] = QJsonObject{};
    auto children = tree.value("children").toObject();
    QString inprocName = "InprocServer32";
    for (auto it = children.begin(); it != children.end(); ++it)
        if (it.key().compare(inprocName, Qt::CaseInsensitive) == 0) { inprocName = it.key(); break; }
    auto inproc = children.value(inprocName).toObject();
    inproc["exists"] = true;
    if (!inproc.contains("children")) inproc["children"] = QJsonObject{};
    auto values = inproc.value("values").toObject();
    values[""] = QJsonObject{{"type", REG_SZ}, {"data", QStringLiteral("AAA=")}};
    inproc["values"] = values;
    children[inprocName] = inproc;
    tree["children"] = children;
    return tree;
}
}

QString productionPath() {
    return QStringLiteral("Software\\Classes\\CLSID\\{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}");
}
QString secondaryProductionPath() {
    return QStringLiteral("Software\\Classes\\CLSID\\{51668040-5e31-4023-bbe3-149163bd80f1}");
}
QString machineBackupKey() { return QStringLiteral("Software\\LethTools\\ContextMenuSwapperV4\\MachineBackup"); }
Registry::Registry(QString path, QString backupPath, Hive hive, bool allowProtectedWrite, QString secondaryPath)
    : path_(std::move(path)), backup_(std::move(backupPath)), secondaryPath_(std::move(secondaryPath)),
      hive_(hive), allowProtectedWrite_(allowProtectedWrite) {}
QString Registry::targets() const {
    const auto prefix = hiveName() + "\\";
    return prefix + path_ + (secondaryPath_.isEmpty() ? QString{} : "\n" + prefix + secondaryPath_);
}
void *Registry::root() const { return machine() ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER; }
QString Registry::backupPath() const {
    return machine() && backup_.isEmpty() ? "HKLM\\" + machineBackupKey() + "\\LastChange" : backup_;
}
State Registry::read() const {
    auto primary = readSingle(path_);
    if (!primary.ok || secondaryPath_.isEmpty()) return primary;
    const auto secondary = readSingle(secondaryPath_);
    if (!secondary.ok) return secondary;
    primary.secondary = secondary.tree;
    return primary;
}
State Registry::readSingle(const QString &target) const {
    Key key;
    const auto path = target.toStdWString();
    const auto rc = RegOpenKeyExW(static_cast<HKEY>(root()), path.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &key.h);
    if (rc == ERROR_FILE_NOT_FOUND || rc == ERROR_PATH_NOT_FOUND) return {true, {{"exists", false}}, {}, {}};
    if (rc != ERROR_SUCCESS) return {false, {}, errorText(rc), {}};
    QJsonObject tree; QString error;
    if (!readTree(key.h, tree, error)) return {false, {}, error, {}};
    return {true, tree, {}, {}};
}
QString State::mode() const {
    if (!ok) return "error";
    if (!secondary.isEmpty()) {
        const auto first = State{true, tree, {}, {}}.mode();
        const auto second = State{true, secondary, {}, {}}.mode();
        return first == second ? first : QStringLiteral("custom");
    }
    if (!tree.value("exists").toBool()) return "modern";
    const auto children = tree.value("children").toObject();
    QJsonObject inproc;
    for (auto it = children.begin(); it != children.end(); ++it)
        if (it.key().compare("InprocServer32", Qt::CaseInsensitive) == 0) { inproc = it.value().toObject(); break; }
    const auto value = inproc.value("values").toObject().value("").toObject();
    if (value.value("type").toInt(-1) == REG_SZ && value.value("data").toString() == "AAA=")
        return "classic";
    return "custom";
}
Result Registry::restore(const QString &target, const QJsonObject &tree) {
    if (!validTree(tree) || target.isEmpty()) return {false, "Invalid snapshot"};
    const auto path = target.toStdWString();
    // HKLM classic only changes the InprocServer32 default. Restore that value in
    // place to retain the original system registration and its access controls.
    if (machine() && tree.value("exists").toBool()) {
        const auto children = tree.value("children").toObject();
        QJsonObject inproc;
        for (auto it = children.begin(); it != children.end(); ++it)
            if (it.key().compare("InprocServer32", Qt::CaseInsensitive) == 0) inproc = it.value().toObject();
        const auto childPath = (target + "\\InprocServer32").toStdWString();
        LSTATUS rc = ERROR_SUCCESS;
        if (!inproc.value("exists").toBool()) {
            rc = RegDeleteTreeW(static_cast<HKEY>(root()), childPath.c_str());
            if (rc == ERROR_FILE_NOT_FOUND || rc == ERROR_PATH_NOT_FOUND) rc = ERROR_SUCCESS;
        } else {
            Key key;
            rc = RegOpenKeyExW(static_cast<HKEY>(root()), childPath.c_str(), 0, KEY_SET_VALUE | KEY_WOW64_64KEY, &key.h);
            if (rc == ERROR_ACCESS_DENIED && allowProtectedWrite_)
                rc = openProtectedValue(static_cast<HKEY>(root()), childPath.c_str(), &key.h);
            if (rc == ERROR_SUCCESS) {
                const auto values = inproc.value("values").toObject();
                if (values.contains("")) {
                    const auto v = values.value("").toObject();
                    const auto bytes = QByteArray::fromBase64(v.value("data").toString().toLatin1());
                    rc = RegSetValueExW(key.h, L"", 0, v.value("type").toInt(),
                        reinterpret_cast<const BYTE *>(bytes.constData()), static_cast<DWORD>(bytes.size()));
                } else {
                    rc = RegDeleteValueW(key.h, L"");
                    if (rc == ERROR_FILE_NOT_FOUND) rc = ERROR_SUCCESS;
                }
            }
        }
        if (rc != ERROR_SUCCESS) return {false, errorText(rc), static_cast<unsigned long>(rc)};
        const auto actual = readSingle(target);
        return {actual.ok && actual.tree == tree, actual.ok ? "HKLM restoration verification" : actual.error};
    }
    auto rc = RegDeleteTreeW(static_cast<HKEY>(root()), path.c_str());
    if (rc != ERROR_SUCCESS && rc != ERROR_FILE_NOT_FOUND && rc != ERROR_PATH_NOT_FOUND)
        return {false, errorText(rc)};
    if (tree.value("exists").toBool()) {
        Key key;
        rc = RegCreateKeyExW(static_cast<HKEY>(root()), path.c_str(), 0, nullptr, 0, KEY_WRITE | KEY_WOW64_64KEY, nullptr, &key.h, nullptr);
        if (rc == ERROR_SUCCESS) rc = writeTree(key.h, tree);
        if (rc != ERROR_SUCCESS) return {false, errorText(rc)};
    }
    const auto actual = readSingle(target);
    return {actual.ok && actual.tree == tree, actual.ok ? QStringLiteral("Registry verification") : actual.error};
}
QJsonObject Registry::backup() const {
    QByteArray data;
    if (machine() && backup_.isEmpty()) {
        Key key;
        const auto p = machineBackupKey().toStdWString();
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, p.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &key.h) != ERROR_SUCCESS) return {};
        DWORD size = 0;
        if (RegGetValueW(key.h, nullptr, L"LastChange", RRF_RT_REG_BINARY, nullptr, nullptr, &size) != ERROR_SUCCESS || size > 16 * 1024 * 1024) return {};
        data.resize(size);
        if (RegGetValueW(key.h, nullptr, L"LastChange", RRF_RT_REG_BINARY, nullptr, data.data(), &size) != ERROR_SUCCESS) return {};
    } else {
        QFile f(backup_);
        if (!f.open(QIODevice::ReadOnly) || f.size() > 16 * 1024 * 1024) return {};
        data = f.readAll();
    }
    const auto record = QJsonDocument::fromJson(data).object();
    const bool legacyUser = record.value("schema").toInt() == 1 && !machine() && !record.contains("hive");
    const bool scoped = record.value("schema").toInt() == 2 && record.value("hive").toString() == hiveName() &&
                        record.value("view").toString() == "64";
    const bool paired = record.value("schema").toInt() == 3 && !secondaryPath_.isEmpty() &&
        record.value("hive").toString() == hiveName() && record.value("view").toString() == "64" &&
        record.value("secondaryPath").toString() == secondaryPath_ &&
        validTree(record.value("secondaryBefore").toObject()) && validTree(record.value("secondaryAfter").toObject());
    if ((!legacyUser && !scoped && !paired) || record.value("path").toString() != path_ ||
        !validTree(record.value("before").toObject()) || !validTree(record.value("after").toObject())) return {};
    return record;
}
bool Registry::canUndo() const {
    const auto r = backup();
    return !r.isEmpty() && r.value("status").toString() != "undone";
}
Result Registry::writeBackup(const QJsonObject &record) const {
    const auto data = QJsonDocument(record).toJson(QJsonDocument::Compact);
    if (machine() && backup_.isEmpty()) {
        // Machine snapshots are never loaded from a user-writable file.
        PSECURITY_DESCRIPTOR descriptor = nullptr;
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"O:BAG:BAD:P(A;CI;KA;;;SY)(A;CI;KA;;;BA)(A;CI;KR;;;BU)", SDDL_REVISION_1, &descriptor, nullptr))
            return {false, errorText(GetLastError())};
        SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), descriptor, FALSE};
        Key key;
        const auto p = machineBackupKey().toStdWString();
        auto rc = RegCreateKeyExW(HKEY_LOCAL_MACHINE, p.c_str(), 0, nullptr, 0,
            KEY_WRITE | KEY_WOW64_64KEY, &attributes, &key.h, nullptr);
        LocalFree(descriptor);
        if (rc == ERROR_SUCCESS) rc = RegSetValueExW(key.h, L"LastChange", 0, REG_BINARY,
            reinterpret_cast<const BYTE *>(data.constData()), static_cast<DWORD>(data.size()));
        return {rc == ERROR_SUCCESS, rc == ERROR_SUCCESS ? QString{} : errorText(rc)};
    }
    if (!QDir().mkpath(QFileInfo(backup_).absolutePath())) return {false, "Cannot create backup directory"};
    QSaveFile file(backup_);
    if (!file.open(QIODevice::WriteOnly)) return {false, file.errorString()};
    if (file.write(data) != data.size() || !file.commit()) return {false, file.errorString()};
    return {true, {}};
}
Result Registry::setClassic(const QString &target, const QJsonObject &before) {
    Key key;
    const auto p = (target + "\\InprocServer32").toStdWString();
    QString stage = "RegOpenKeyExW (KEY_SET_VALUE)";
    auto rc = RegOpenKeyExW(static_cast<HKEY>(root()), p.c_str(), 0,
                           KEY_SET_VALUE | KEY_WOW64_64KEY, &key.h);
    // Existing system registrations may allow updating a value but forbid
    // creating children. Do not request KEY_WRITE/KEY_CREATE_SUB_KEY here.
    if (rc == ERROR_FILE_NOT_FOUND || rc == ERROR_PATH_NOT_FOUND) {
        stage = "RegCreateKeyExW (KEY_SET_VALUE)";
        rc = RegCreateKeyExW(static_cast<HKEY>(root()), p.c_str(), 0, nullptr, 0,
                            KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &key.h, nullptr);
    }
    bool existed = false;
    const auto children = before.value("children").toObject();
    for (auto it = children.begin(); it != children.end(); ++it)
        if (it.key().compare("InprocServer32", Qt::CaseInsensitive) == 0) existed = true;
    if (rc == ERROR_ACCESS_DENIED && machine() && allowProtectedWrite_ && existed) {
        stage = "RegCreateKeyExW (REG_OPTION_BACKUP_RESTORE / SeRestorePrivilege)";
        rc = openProtectedValue(static_cast<HKEY>(root()), p.c_str(), &key.h);
    }
    const wchar_t empty[] = L"";
    if (rc == ERROR_SUCCESS) {
        stage = "RegSetValueExW (default / par défaut)";
        rc = RegSetValueExW(key.h, L"", 0, REG_SZ, reinterpret_cast<const BYTE *>(empty), sizeof(empty));
    }
    return {rc == ERROR_SUCCESS, rc == ERROR_SUCCESS ? QString{} :
        QStringLiteral("Étape / Step: %1\nCible / Target: %2\\InprocServer32\n%3")
            .arg(stage, hiveName() + "\\" + target, errorText(rc)), static_cast<unsigned long>(rc)};
}
Result Registry::apply(bool classic) {
    if (machine() && !classic) return {false, "HKLM: use Undo to restore the saved system registration; deletion is refused."};
    const auto before = read();
    if (!before.ok) return {false, before.error};
    if (!validTree(before.tree) || QJsonDocument(before.tree).toJson(QJsonDocument::Compact).size() > 4 * 1024 * 1024)
        return {false, "Registry subtree exceeds backup safety limits"};
    const auto after = classic ? classicTree(before.tree) : QJsonObject{{"exists", false}};
    const bool paired = !secondaryPath_.isEmpty();
    if (paired && (!validTree(before.secondary) ||
        QJsonDocument(before.secondary).toJson(QJsonDocument::Compact).size() > 3 * 1024 * 1024 ||
        QJsonDocument(before.tree).toJson(QJsonDocument::Compact).size() > 3 * 1024 * 1024))
        return {false, "Registry subtrees exceed backup safety limits"};
    const auto secondaryAfter = !paired ? QJsonObject{} :
        (classic ? classicTree(before.secondary) : QJsonObject{{"exists", false}});
    if (before.tree == after && before.secondary == secondaryAfter) return {true, "unchanged"};
    auto previous = backup();
    if (!previous.isEmpty() && previous.value("status").toString() == "prepared")
        return {false, "An interrupted operation requires Undo before another change."};
    QJsonObject record{{"schema", 2}, {"hive", hiveName()}, {"view", "64"}, {"path", path_}, {"before", before.tree}, {"after", after},
        {"protectedWrite", machine() && allowProtectedWrite_},
        {"status", "prepared"}, {"time", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
    if (paired) {
        record["schema"] = 3;
        record["secondaryPath"] = secondaryPath_;
        record["secondaryBefore"] = before.secondary;
        record["secondaryAfter"] = secondaryAfter;
    }
    auto result = writeBackup(record);
    if (!result.ok) return result; // No registry write unless the snapshot is durable.
    const auto current = read();
    if (!current.ok || (current.tree != before.tree || current.secondary != before.secondary)) {
        record["status"] = "undone";
        writeBackup(record);
        return {false, "Registry changed externally; refresh and try again."};
    }
    if (before.tree != after)
        result = classic ? setClassic(path_, before.tree) : restore(path_, after);
    if (result.ok && paired && before.secondary != secondaryAfter)
        result = classic ? setClassic(secondaryPath_, before.secondary) : restore(secondaryPath_, secondaryAfter);
    const auto actual = read();
    if (!result.ok || !actual.ok || actual.tree != after || actual.secondary != secondaryAfter) {
        const bool unchanged = actual.ok && actual.tree == before.tree && actual.secondary == before.secondary;
        Result rollbackSecondary{true, {}};
        Result rollbackPrimary{true, {}};
        if (paired && (!actual.ok || actual.secondary != before.secondary))
            rollbackSecondary = restore(secondaryPath_, before.secondary);
        if (!actual.ok || actual.tree != before.tree)
            rollbackPrimary = restore(path_, before.tree);
        const bool rolledBack = rollbackPrimary.ok && rollbackSecondary.ok;
        if (rolledBack) { record["status"] = "undone"; writeBackup(record); }
        const auto previousState = rolledBack ?
            (unchanged ? QStringLiteral("inchangée / unchanged") : QStringLiteral("restaurée / restored")) :
            QStringLiteral("Restauration incomplète / Incomplete restoration: %1 %2").arg(rollbackPrimary.message, rollbackSecondary.message);
        return {false, QStringLiteral("Application impossible / Apply failed\n%1\n\nConfiguration précédente / Previous state: %2\nSauvegarde / Backup: %3")
            .arg(result.message, previousState, backupPath()), result.windowsError};
    }
    record["status"] = "applied";
    result = writeBackup(record);
    if (!result.ok) return {false, "Registry changed, but backup finalization failed: " + result.message};
    return {true, "changed"};
}
Result Registry::undo() {
    auto record = backup();
    if (record.isEmpty() || record.value("status").toString() == "undone") return {false, "No valid backup"};
    const auto current = read();
    if (!current.ok) return {false, current.error};
    const auto before = record.value("before").toObject(), after = record.value("after").toObject();
    if (machine() && record.value("protectedWrite").toBool()) allowProtectedWrite_ = true;
    const bool paired = record.value("schema").toInt() == 3;
    const auto secondaryBefore = record.value("secondaryBefore").toObject();
    const auto secondaryAfter = record.value("secondaryAfter").toObject();
    const bool atBefore = current.tree == before && (!paired || current.secondary == secondaryBefore);
    const bool atAfter = current.tree == after && (!paired || current.secondary == secondaryAfter);
    const bool interrupted = record.value("status") == "prepared";
    const bool recoverable = interrupted && (current.tree == before || current.tree == after) &&
        (!paired || current.secondary == secondaryBefore || current.secondary == secondaryAfter);
    if (!atBefore && !atAfter && !recoverable)
        return {false, "Registry changed externally. Automatic restore refused; backup preserved."};
    if (!atBefore) {
        // Mark the operation recoverable before restoring either key.
        record["status"] = "prepared";
        const auto prepared = writeBackup(record);
        if (!prepared.ok) return prepared;
        if (paired && current.secondary != secondaryBefore) {
            const auto result = restore(secondaryPath_, secondaryBefore);
            if (!result.ok) return result;
        }
        if (current.tree != before) {
            const auto result = restore(path_, before);
            if (!result.ok) return result;
        }
    }
    record["status"] = "undone";
    auto result = writeBackup(record);
    return result.ok ? Result{true, atBefore ? "unchanged" : "changed"} : result;
}
QString systemDescription() {
    Key key;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0,
        KEY_READ | KEY_WOW64_64KEY, &key.h) != ERROR_SUCCESS) return "Windows · version unavailable";
    auto value = [&](const wchar_t *name) {
        wchar_t data[256]{}; DWORD size = sizeof(data);
        if (RegGetValueW(key.h, nullptr, name, RRF_RT_REG_SZ, nullptr, data, &size) != ERROR_SUCCESS) return QString{};
        return QString::fromWCharArray(data);
    };
    const auto build = value(L"CurrentBuildNumber");
    auto name = value(L"ProductName");
    const bool client = value(L"InstallationType").compare("Client", Qt::CaseInsensitive) == 0;
    // Windows 11 still exposes "Windows 10 Pro/Home" in ProductName on many systems.
    if (client && build.toInt() >= 22000 && name.startsWith("Windows 10")) name.replace(0, 10, "Windows 11");
    if (name.isEmpty()) name = client && build.toInt() >= 22000 ? "Windows 11" : "Windows";
    auto release = value(L"DisplayVersion");
    if (release.isEmpty()) release = value(L"ReleaseId");
    DWORD revision = 0, size = sizeof(revision);
    const bool hasRevision = RegGetValueW(key.h, nullptr, L"UBR", RRF_RT_REG_DWORD, nullptr,
                                         &revision, &size) == ERROR_SUCCESS;
    SYSTEM_INFO info{}; GetNativeSystemInfo(&info);
    const auto architecture = info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ? "x64" :
        info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64 ? "ARM64" :
        info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL ? "x86" : "unknown architecture";
    QStringList fields{name};
    if (!release.isEmpty()) fields << release;
    if (!build.isEmpty()) fields << "build " + build + (hasRevision ? "." + QString::number(revision) : QString{});
    fields << architecture;
    return fields.join(QString::fromUtf8(" · "));
}
}
