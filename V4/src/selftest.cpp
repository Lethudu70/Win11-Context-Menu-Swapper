#include "registry.h"
#include "machine.h"
#include "cache.h"
#include <windows.h>
#include <sddl.h>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QUuid>
#include <functional>
#include <cstring>

namespace cms {
namespace {
DWORD restorePrivilegeState() {
    HANDLE token = nullptr; LUID luid{};
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return 0xffffffff;
    DWORD size = 0; GetTokenInformation(token, TokenPrivileges, nullptr, 0, &size);
    QByteArray data(size, '\0'); DWORD result = 0xffffffff;
    if (LookupPrivilegeValueW(nullptr, SE_RESTORE_NAME, &luid) && GetTokenInformation(token, TokenPrivileges, data.data(), size, &size)) {
        const auto *privileges = reinterpret_cast<const TOKEN_PRIVILEGES *>(data.constData());
        for (DWORD i = 0; i < privileges->PrivilegeCount; ++i)
            if (privileges->Privileges[i].Luid.LowPart == luid.LowPart && privileges->Privileges[i].Luid.HighPart == luid.HighPart)
                result = privileges->Privileges[i].Attributes;
    }
    CloseHandle(token); return result;
}
void cacheTests(const QString &directory, const std::function<void(const QString &, bool)> &check) {
    const QDir dir(directory + "/json-cache"); QDir().mkpath(dir.path());
    Registry registry("Software\\LethTools\\TestOnly", dir.filePath("last-change.json"));
    auto write = [&](const QString &name, const QByteArray &bytes) {
        QSaveFile file(dir.filePath(name));
        return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
    };
    QJsonObject record{{"schema", 1}, {"path", registry.path()}, {"before", QJsonObject{{"exists", false}}},
        {"after", QJsonObject{{"exists", false}}}, {"status", "prepared"}};
    const auto active = QJsonDocument(record).toJson();
    check("JSON cleanup fixtures created", write("cache.json", "{}") && write("preferences.json", "{\"keep\":true}") &&
        write("last-change.json", active));
    QDir().mkpath(dir.filePath("nested"));
    check("nested JSON cleanup fixture created", write("nested/cache.json", "{}"));
    const auto first = cleanJsonCache(dir.path(), registry);
    check("cleanup removes recognized cache only", first.ok() && first.removed == 1 && !QFileInfo::exists(dir.filePath("cache.json")));
    check("cleanup preserves active undo by default", QFileInfo::exists(registry.backupPath()) && registry.backup().value("status") == "prepared");
    check("cleanup preserves configuration and nested files", QFileInfo::exists(dir.filePath("preferences.json")) &&
        QFileInfo::exists(dir.filePath("nested/cache.json")));
    const auto reset = cleanJsonCache(dir.path(), registry, true);
    const auto archived = QDir(dir.filePath("archive")).entryList({"*.bak"}, QDir::Files);
    QFile archivedFile(archived.isEmpty() ? QString{} : QDir(dir.filePath("archive")).filePath(archived.first()));
    const bool archivedRead = archivedFile.open(QIODevice::ReadOnly);
    check("explicit undo reset archives the original bytes", reset.ok() && reset.archived == 1 &&
        !QFileInfo::exists(registry.backupPath()) && archivedRead && archivedFile.readAll() == active);
    archivedFile.close();
    record["status"] = "undone"; check("finished history fixture created", write("last-change.json", QJsonDocument(record).toJson()));
    const auto finished = cleanJsonCache(dir.path(), registry);
    check("cleanup removes finished undo history", finished.ok() && finished.removed == 1);
    check("corrupt JSON fixture created", write("last-change.json", "{broken"));
    const auto broken = cleanJsonCache(dir.path(), registry);
    check("corrupt undo JSON is archived before reset", broken.ok() && broken.archived == 1 && !QFileInfo::exists(registry.backupPath()));
    Registry machine("Software\\LethTools\\TestOnly", registry.backupPath(), Hive::LocalMachine);
    check("JSON cleanup refuses machine storage", !cleanJsonCache(dir.path(), machine, true).ok());
}
void machineTests(const QString &directory, const std::function<void(const QString &, bool)> &check) {
    check("HKLM helper allowlist accepts classic and undo only", validMachineAction("classic") && validMachineAction("undo") &&
        !validMachineAction("modern") && !validMachineAction("classic --path anything"));
    const auto fixturePath = "Software\\LethTools\\ContextMenuSwapperMachineTests\\" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    HKEY fixture = nullptr;
    const auto create = RegCreateKeyExW(HKEY_CURRENT_USER, fixturePath.toStdWString().c_str(), 0, nullptr, 0,
        KEY_ALL_ACCESS | KEY_WOW64_64KEY, nullptr, &fixture, nullptr);
    check("isolated machine fixture created under HKCU", create == ERROR_SUCCESS);
    if (create != ERROR_SUCCESS) return;
    const auto redirect = RegOverridePredefKey(HKEY_LOCAL_MACHINE, fixture);
    check("HKLM redirected for this test process only", redirect == ERROR_SUCCESS);
    if (redirect == ERROR_SUCCESS) {
        struct Reset { ~Reset() { RegOverridePredefKey(HKEY_LOCAL_MACHINE, nullptr); } } reset;
        Registry machine(productionPath(), directory + "/machine.json", Hive::LocalMachine);
        const auto absent = machine.read();
        check("machine default is isolated and absent", absent.ok && absent.mode() == "modern");
        check("unbacked HKLM deletion is refused", !machine.apply(false).ok && machine.read().tree == absent.tree);
        check("HKLM classic applies to redirected machine hive", machine.apply(true).ok && machine.read().mode() == "classic");
        const auto snapshot = machine.backup();
        check("machine snapshot binds hive and registry view", snapshot.value("hive") == "HKLM" && snapshot.value("view") == "64");
        Registry wrongHive(productionPath(), machine.backupPath(), Hive::CurrentUser);
        check("HKLM backup cannot be restored as HKCU", !wrongHive.canUndo() && !wrongHive.undo().ok);
        check("machine undo removes only newly created override", machine.undo().ok && machine.read().tree == absent.tree);
        Registry pairedMachine(productionPath(), directory + "/paired-success.json", Hive::LocalMachine,
                               false, secondaryProductionPath());
        check("HKLM sets both defaults in the redirected hive", pairedMachine.apply(true).ok &&
            pairedMachine.read().mode() == "classic");
        check("HKLM undo restores both redirected registrations", pairedMachine.undo().ok &&
            pairedMachine.read().mode() == "modern");
        HKEY original = nullptr;
        const auto child = (productionPath() + "\\InprocServer32").toStdWString();
        auto rc = RegCreateKeyExW(HKEY_LOCAL_MACHINE, child.c_str(), 0, nullptr, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, nullptr, &original, nullptr);
        check("original system registration fixture created", rc == ERROR_SUCCESS);
        QByteArray securityBefore;
        if (rc == ERROR_SUCCESS) {
            const wchar_t dll[] = L"%SystemRoot%\\system-original.dll";
            const wchar_t model[] = L"Apartment";
            check("machine default fixture written", RegSetValueExW(original, L"", 0, REG_EXPAND_SZ,
                reinterpret_cast<const BYTE *>(dll), sizeof(dll)) == ERROR_SUCCESS);
            check("unrelated machine value fixture written", RegSetValueExW(original, L"ThreadingModel", 0, REG_SZ,
                reinterpret_cast<const BYTE *>(model), sizeof(model)) == ERROR_SUCCESS);
            DWORD size = 0;
            RegGetKeySecurity(original, DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION, nullptr, &size);
            securityBefore.resize(size);
            check("original machine ACL captured", RegGetKeySecurity(original, DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION,
                reinterpret_cast<PSECURITY_DESCRIPTOR>(securityBefore.data()), &size) == ERROR_SUCCESS);
            RegCloseKey(original);
        }
        const auto before = machine.read();
        check("HKLM classic preserves original registration values", machine.apply(true).ok && machine.read().mode() == "classic");
        check("HKLM undo restores original server registration", machine.undo().ok && machine.read().tree == before.tree);
        rc = RegOpenKeyExW(HKEY_LOCAL_MACHINE, child.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &original);
        QByteArray securityAfter(securityBefore.size(), '\0'); DWORD size = static_cast<DWORD>(securityAfter.size());
        const bool securityOk = rc == ERROR_SUCCESS && RegGetKeySecurity(original, DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION,
            reinterpret_cast<PSECURITY_DESCRIPTOR>(securityAfter.data()), &size) == ERROR_SUCCESS;
        if (rc == ERROR_SUCCESS) RegCloseKey(original);
        check("HKLM undo preserves the original ACL and owner", securityOk && !securityBefore.isEmpty() && securityAfter == securityBefore);
        // Reproduce access denied caused solely by requesting unnecessary rights:
        // allow reading and setting values, explicitly deny creating children.
        HANDLE token = nullptr; DWORD tokenSize = 0;
        const bool tokenOpened = OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
        if (tokenOpened) GetTokenInformation(token, TokenUser, nullptr, 0, &tokenSize);
        QByteArray tokenData(tokenSize, '\0');
        const bool userKnown = tokenOpened && GetTokenInformation(token, TokenUser, tokenData.data(), tokenSize, &tokenSize);
        wchar_t *sid = nullptr;
        const bool sidKnown = userKnown && ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(tokenData.data())->User.Sid, &sid);
        check("restricted registry fixture user resolved", sidKnown);
        if (token) CloseHandle(token);
        if (sidKnown) {
            const auto sddl = (QStringLiteral("D:P(D;;0x0004;;;WD)(A;;0x0002001B;;;") + QString::fromWCharArray(sid) + ")").toStdWString();
            PSECURITY_DESCRIPTOR descriptor = nullptr;
            const bool descriptorOk = ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &descriptor, nullptr);
            rc = RegOpenKeyExW(HKEY_LOCAL_MACHINE, child.c_str(), 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &original);
            const bool installed = descriptorOk && rc == ERROR_SUCCESS && RegSetKeySecurity(original,
                DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, descriptor) == ERROR_SUCCESS;
            check("value-only registry permissions installed", installed);
            if (installed) {
                HKEY broad = nullptr;
                const auto broadResult = RegOpenKeyExW(HKEY_LOCAL_MACHINE, child.c_str(), 0, KEY_WRITE | KEY_WOW64_64KEY, &broad);
                if (broad) RegCloseKey(broad);
                check("old broad write request reproduces access denied", broadResult == ERROR_ACCESS_DENIED);
                check("minimal value-only request applies successfully", machine.apply(true).ok && machine.read().mode() == "classic");
                check("minimal value-only undo restores original data", machine.undo().ok && machine.read().tree == before.tree);
                DWORD restrictedSize = 0;
                RegGetKeySecurity(original, DACL_SECURITY_INFORMATION, nullptr, &restrictedSize);
                QByteArray restricted(restrictedSize, '\0');
                const bool securityRead = RegGetKeySecurity(original, DACL_SECURITY_INFORMATION,
                    reinterpret_cast<PSECURITY_DESCRIPTOR>(restricted.data()), &restrictedSize) == ERROR_SUCCESS;
                BOOL present = FALSE, defaulted = FALSE; PACL expectedAcl = nullptr, actualAcl = nullptr;
                const bool descriptorsValid = GetSecurityDescriptorDacl(descriptor, &present, &expectedAcl, &defaulted) && present &&
                    securityRead && GetSecurityDescriptorDacl(reinterpret_cast<PSECURITY_DESCRIPTOR>(restricted.data()), &present, &actualAcl, &defaulted) && present;
                check("value-only operation does not relax registry permissions", descriptorsValid && expectedAcl && actualAcl &&
                    expectedAcl->AclSize == actualAcl->AclSize && memcmp(expectedAcl, actualAcl, expectedAcl->AclSize) == 0);
                PSECURITY_DESCRIPTOR deniedDescriptor = nullptr;
                const auto deniedSddl = (QStringLiteral("D:P(D;;0x0006;;;WD)(A;;0x00020019;;;") + QString::fromWCharArray(sid) + ")").toStdWString();
                const bool deniedInstalled = ConvertStringSecurityDescriptorToSecurityDescriptorW(deniedSddl.c_str(), SDDL_REVISION_1,
                    &deniedDescriptor, nullptr) && RegSetKeySecurity(original, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                        deniedDescriptor) == ERROR_SUCCESS;
                check("fully protected value fixture installed", deniedInstalled);
                if (deniedInstalled) {
                    const auto firstPath = productionPath() + "-paired-fixture";
                    Registry paired(firstPath, directory + "/paired-machine.json", Hive::LocalMachine, false, productionPath());
                    const auto pairedBefore = paired.read();
                    const auto pairedFailure = paired.apply(true);
                    const auto pairedAfter = paired.read();
                    check("denial on the second HKLM key rolls back the first",
                        !pairedFailure.ok && pairedFailure.windowsError == ERROR_ACCESS_DENIED &&
                        pairedAfter.ok && pairedAfter.tree == pairedBefore.tree && pairedAfter.secondary == pairedBefore.secondary);
                    const auto deniedResult = machine.apply(true);
                    check("genuine write denial preserves the previous state", !deniedResult.ok && machine.read().tree == before.tree);
                    check("write denial identifies operation target and backup", deniedResult.message.contains("RegOpenKeyExW") &&
                        deniedResult.message.contains("HKLM\\") && deniedResult.message.contains(machine.backupPath()) &&
                        deniedResult.message.contains("unchanged"));
                    const auto privilegeBefore = restorePrivilegeState();
                    Registry advanced(productionPath(), directory + "/advanced-machine.json", Hive::LocalMachine, true);
                    const auto advancedResult = advanced.apply(true);
                    if (advancedResult.ok) {
                        check("restore privilege can write a protected fixture", advanced.read().mode() == "classic" && advanced.backup().value("protectedWrite").toBool());
                        check("protected fixture undo restores original data", advanced.undo().ok && advanced.read().tree == before.tree);
                    } else {
                        check("missing restore privilege fails closed", (advancedResult.windowsError == ERROR_NOT_ALL_ASSIGNED ||
                            advancedResult.windowsError == ERROR_PRIVILEGE_NOT_HELD || advancedResult.windowsError == ERROR_ACCESS_DENIED) &&
                            advanced.read().tree == before.tree);
                    }
                    check("restore privilege returns to its previous state", restorePrivilegeState() == privilegeBefore);
                }
                if (deniedDescriptor) LocalFree(deniedDescriptor);
                SECURITY_DESCRIPTOR_CONTROL control{}; DWORD revision = 0;
                const bool controlKnown = GetSecurityDescriptorControl(reinterpret_cast<PSECURITY_DESCRIPTOR>(securityBefore.data()), &control, &revision);
                const auto flags = DACL_SECURITY_INFORMATION | ((controlKnown && (control & SE_DACL_PROTECTED)) ?
                    PROTECTED_DACL_SECURITY_INFORMATION : UNPROTECTED_DACL_SECURITY_INFORMATION);
                check("test-only registry permissions restored", RegSetKeySecurity(original, flags,
                    reinterpret_cast<PSECURITY_DESCRIPTOR>(securityBefore.data())) == ERROR_SUCCESS);
            }
            if (rc == ERROR_SUCCESS) RegCloseKey(original);
            if (descriptor) LocalFree(descriptor);
            LocalFree(sid);
        }
        Registry invalid(productionPath(), directory, Hive::LocalMachine);
        check("machine backup failure prevents mutation", !invalid.apply(true).ok && machine.read().tree == before.tree);
        // This also exercises the protected registry-backed snapshot in the redirected hive.
        Registry protectedBackup(productionPath(), {}, Hive::LocalMachine);
        check("machine backup has a nonempty diagnostic location", protectedBackup.backupPath().startsWith("HKLM\\") &&
            protectedBackup.backupPath().endsWith("LastChange"));
        const auto result = protectedBackup.apply(true);
        if (isProcessElevated()) {
            check("protected machine backup writes when elevated", result.ok && protectedBackup.canUndo());
            check("protected machine backup restores", protectedBackup.undo().ok && protectedBackup.read().tree == before.tree);
        } else {
            check("protected machine backup fails closed without elevation", !result.ok && protectedBackup.read().tree == before.tree);
        }
    }
    RegCloseKey(fixture);
    check("isolated machine fixture removed", RegDeleteTreeW(HKEY_CURRENT_USER, fixturePath.toStdWString().c_str()) == ERROR_SUCCESS);
}
}
int selfTest(const QString &reportPath) {
    QTemporaryDir temp;
    const auto path = QStringLiteral("Software\\LethTools\\ContextMenuSwapperTests\\") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto wide = path.toStdWString();
    Registry registry(path, temp.path() + "/backup.json");
    QJsonArray checks;
    bool passed = temp.isValid();
    auto check = [&](const QString &name, bool ok) {
        checks.append(QJsonObject{{"name", name}, {"passed", ok}}); passed = passed && ok;
    };
    check("production CLSID is not addressed", path != productionPath());
    check("absent key detected as Windows default", registry.read().mode() == "modern");
    check("classic apply verified", registry.apply(true).ok && registry.read().mode() == "classic");
    check("no-op keeps undo available", registry.apply(true).message == "unchanged" && registry.canUndo());
    check("undo restores key absence", registry.undo().ok && !registry.read().tree.value("exists").toBool());
    check("second undo refused", !registry.undo().ok);
    HKEY key = nullptr;
    auto rc = RegCreateKeyExW(HKEY_CURRENT_USER, (path + "\\inprocserver32").toStdWString().c_str(),
        0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &key, nullptr);
    check("custom fixture created", rc == ERROR_SUCCESS);
    const wchar_t original[] = L"original.dll";
    const BYTE binary[]{0, 1, 0, 255, 32};
    if (key) {
        RegSetValueExW(key, L"", 0, REG_EXPAND_SZ, reinterpret_cast<const BYTE *>(original), sizeof(original));
        RegSetValueExW(key, L"BinaryValue", 0, REG_BINARY, binary, sizeof(binary));
        RegCloseKey(key);
    }
    const auto custom = registry.read();
    check("custom configuration recognized", custom.mode() == "custom");
    check("classic preserves existing child and binary values", registry.apply(true).ok && registry.read().mode() == "classic");
    check("undo preserves original raw values and types", registry.undo().ok && registry.read().tree == custom.tree);
    check("modern removes only target subtree", registry.apply(false).ok && registry.read().mode() == "modern");
    check("undo restores nested custom subtree", registry.undo().ok && registry.read().tree == custom.tree);
    check("apply before conflict", registry.apply(true).ok);
    rc = RegOpenKeyExW(HKEY_CURRENT_USER, wide.c_str(), 0, KEY_WRITE, &key);
    if (rc == ERROR_SUCCESS) {
        DWORD number = 42;
        RegSetValueExW(key, L"external", 0, REG_DWORD, reinterpret_cast<BYTE *>(&number), sizeof(number));
        RegCloseKey(key);
    }
    const auto conflict = registry.read();
    check("external changes prevent undo", !registry.undo().ok && registry.read().tree == conflict.tree);
    // Corruption must fail closed and leave registry unchanged.
    QFile backup(registry.backupPath());
    check("corrupt fixture writable", backup.open(QIODevice::WriteOnly));
    check("corrupt fixture written", backup.write("{corrupt") == 8); backup.close();
    check("corrupt backup refused", !registry.canUndo() && !registry.undo().ok && registry.read().tree == conflict.tree);
    Registry invalid(path, temp.path()); // A directory cannot be opened as a backup file.
    check("backup failure prevents registry write", !invalid.apply(false).ok && registry.read().tree == conflict.tree);
    // Interrupted finalization: prepared backup + fully applied state can be recovered after restart.
    check("apply for interruption recovery", registry.apply(false).ok);
    auto record = registry.backup(); record["status"] = "prepared";
    check("interruption fixture writable", backup.open(QIODevice::WriteOnly));
    const auto interruptedBytes = QJsonDocument(record).toJson();
    check("interruption fixture written", backup.write(interruptedBytes) == interruptedBytes.size()); backup.close();
    Registry reopened(path, registry.backupPath());
    check("interrupted operation blocks next mutation", !reopened.apply(true).ok);
    check("interrupted operation recovered", reopened.undo().ok && reopened.read().tree == conflict.tree);
    const auto cleanup = RegDeleteTreeW(HKEY_CURRENT_USER, wide.c_str());
    check("temporary test key cleaned", cleanup == ERROR_SUCCESS);
    const auto secondary = path + "-second";
    Registry paired(path, temp.path() + "/paired.json", Hive::CurrentUser, false, secondary);
    Registry old(path, paired.backupPath());
    check("existing V4.0 backup can still be undone",
        old.apply(true).ok && paired.read().mode() == "custom" && paired.canUndo() &&
        paired.undo().ok && paired.read().mode() == "modern");
    check("classic writes both isolated HKCU defaults",
        paired.apply(true).ok && paired.read().mode() == "classic" && paired.backup().value("schema") == 3);
    const auto pairedClassic = paired.read();
    check("modern removes both isolated overrides", paired.apply(false).ok && paired.read().mode() == "modern");
    check("undo restores both overrides", paired.undo().ok && paired.read().tree == pairedClassic.tree &&
        paired.read().secondary == pairedClassic.secondary);
    check("dual-key test data removed", paired.apply(false).ok && paired.read().mode() == "modern");
    machineTests(temp.path(), check);
    cacheTests(temp.path(), check);
    const QJsonObject report{{"passed", passed}, {"isolatedKey", path}, {"checks", checks},
        {"productionMenuModified", false}};
    const auto bytes = QJsonDocument(report).toJson(QJsonDocument::Indented);
    if (!reportPath.isEmpty()) {
        QDir().mkpath(QFileInfo(reportPath).absolutePath());
        QSaveFile file(reportPath);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) return 2;
    }
    QFile out;
    if (out.open(stdout, QIODevice::WriteOnly)) { out.write(bytes); out.flush(); }
    return passed ? 0 : 1;
}
}
