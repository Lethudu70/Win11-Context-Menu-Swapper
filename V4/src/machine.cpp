#include "machine.h"
#include "registry.h"
#include <windows.h>
#include <shellapi.h>
#include <objbase.h>
#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QIcon>
#include <QMessageBox>

namespace cms {
namespace {
QString checkboxStyle() {
    return "QCheckBox {spacing:9px;} QCheckBox::indicator {width:18px;height:18px;border:2px solid #49785e;border-radius:4px;background:#ffffff;}"
           "QCheckBox::indicator:checked {background:#66eea2;border-color:#216940;image:url(:/checkmark.png);}"
           "QCheckBox::indicator:hover {border-color:#138744;}";
}
}
bool validMachineAction(const QString &action) { return action == "classic" || action == "undo"; }
bool isProcessElevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elevation{}; DWORD size = 0;
    const bool ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size);
    CloseHandle(token);
    return ok && elevation.TokenIsElevated;
}
ElevationLaunch launchMachineAction(const QString &action, bool french, void *parentWindow) {
    if (!validMachineAction(action)) return {nullptr, ERROR_INVALID_PARAMETER};
    const auto exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath()).toStdWString();
    const auto args = ("--machine-action " + action + " --machine-language " + (french ? "fr" : "en")).toStdWString();
    const auto directory = QDir::toNativeSeparators(QCoreApplication::applicationDirPath()).toStdWString();
    const auto com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    SHELLEXECUTEINFOW request{};
    request.cbSize = sizeof(request);
    request.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    request.hwnd = static_cast<HWND>(parentWindow);
    request.lpVerb = L"runas";
    request.lpFile = exe.c_str(); request.lpParameters = args.c_str(); request.lpDirectory = directory.c_str();
    request.nShow = SW_SHOWNORMAL;
    const bool ok = ShellExecuteExW(&request);
    const auto error = ok ? ERROR_SUCCESS : GetLastError();
    if (SUCCEEDED(com)) CoUninitialize();
    return {ok ? request.hProcess : nullptr, ok && !request.hProcess ? ERROR_INVALID_HANDLE : error};
}
int runMachineAction(const QString &action, bool french) {
    auto t = [french](const char *fr, const char *en) { return QString::fromUtf8(french ? fr : en); };
    if (!validMachineAction(action)) return 31;
    if (!isProcessElevated()) {
        QMessageBox::warning(nullptr, "HKLM", t("Droits administrateur requis. Utilisez le bouton HKLM de l'application pour ouvrir l'UAC.",
            "Administrator rights required. Use the app's HKLM button to open UAC."));
        return 30;
    }
    // This helper accepts only two fixed operations on the fixed production CLSID.
    // No caller-controlled key, hive, executable or backup file is accepted.
    SetLastError(ERROR_SUCCESS);
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Global\\LethTools.ContextMenuSwapperV4.HKLM");
    const auto error = GetLastError();
    if (!mutex || error == ERROR_ALREADY_EXISTS) {
        if (mutex) CloseHandle(mutex);
        QMessageBox::warning(nullptr, "HKLM", t("Une opération HKLM est déjà en cours ou le verrou est inaccessible.",
            "An HKLM operation is already running or its lock is inaccessible."));
        return 30;
    }
    struct Release { HANDLE h; ~Release() { ReleaseMutex(h); CloseHandle(h); } } release{mutex};
    const bool undo = action == "undo";
    QMessageBox box(QMessageBox::Warning, t("HKLM — EXPÉRIMENTAL", "HKLM — EXPERIMENTAL"),
        t("EXPERIMENTAL — NON RECOMMANDÉ POUR LES UTILISATEURS STANDARD.\n\nCette opération peut affecter tous les comptes du PC. Son effet n'est pas garanti et un réglage HKCU peut rester prioritaire. Une clé protégée peut nécessiter une confirmation avancée ; propriétaire et permissions sont conservés.\n\n",
          "EXPERIMENTAL — NOT RECOMMENDED FOR STANDARD USERS.\n\nThis operation can affect all accounts on this PC. Its effect is not guaranteed and an HKCU override may take precedence. A protected key may require advanced confirmation; ownership and permissions are preserved.\n\n") +
        (undo ? t("Action : restaurer l'état HKLM sauvegardé.", "Action: restore the saved HKLM state.") :
                t("Action : sauvegarder HKLM, puis définir l'override classique.", "Action: back up HKLM, then set the classic override.")) +
        "\n\nHKLM\\" + productionPath(), QMessageBox::Yes | QMessageBox::Cancel);
    box.setWindowIcon(QIcon(":/app.ico"));
    box.setStyleSheet(checkboxStyle());
    auto *ack = new QCheckBox(t("Je comprends la portée système et le caractère expérimental.",
                                "I understand the system-wide scope and experimental nature."));
    box.setCheckBox(ack);
    auto *yes = box.button(QMessageBox::Yes);
    yes->setText(t("Confirmer l'opération HKLM", "Confirm HKLM operation")); yes->setEnabled(false);
    box.button(QMessageBox::Cancel)->setText(t("Annuler", "Cancel"));
    box.setDefaultButton(QMessageBox::Cancel);
    QObject::connect(ack, &QCheckBox::toggled, yes, &QAbstractButton::setEnabled);
    if (box.exec() != QMessageBox::Yes || !ack->isChecked()) return 20;
    Registry registry(productionPath(), {}, Hive::LocalMachine);
    auto result = undo ? registry.undo() : registry.apply(true);
    if (!undo && !result.ok && result.windowsError == ERROR_ACCESS_DENIED && registry.backup().value("status") == "undone") {
        QMessageBox retry(QMessageBox::Warning, t("HKLM — clé protégée", "HKLM — protected key"),
            t("L'UAC est accepté, mais Windows protège cette valeur.\n\nRéessayer avec le privilège de restauration Windows (SeRestorePrivilege) ? Il sera activé uniquement pendant l'ouverture de la clé, puis remis dans son état précédent. Le propriétaire et les permissions ne seront pas modifiés.\n\nCette tentative avancée reste expérimentale.",
              "UAC was accepted, but Windows protects this value.\n\nRetry using the Windows restore privilege (SeRestorePrivilege)? It will be enabled only while opening the key, then restored to its previous state. Ownership and permissions will not be changed.\n\nThis advanced attempt remains experimental."), QMessageBox::Yes | QMessageBox::Cancel);
        retry.setWindowIcon(QIcon(":/app.ico")); retry.setStyleSheet(checkboxStyle());
        retry.setDetailedText(result.message);
        auto *consent = new QCheckBox(t("J'autorise cette tentative avancée sur la clé HKLM ciblée.", "I authorize this advanced attempt on the targeted HKLM key."));
        retry.setCheckBox(consent);
        retry.button(QMessageBox::Yes)->setText(t("Réessayer sur la clé protégée", "Retry protected-key write"));
        retry.button(QMessageBox::Yes)->setEnabled(false);
        retry.button(QMessageBox::Cancel)->setText(t("Annuler", "Cancel")); retry.setDefaultButton(QMessageBox::Cancel);
        QObject::connect(consent, &QCheckBox::toggled, retry.button(QMessageBox::Yes), &QAbstractButton::setEnabled);
        if (retry.exec() != QMessageBox::Yes || !consent->isChecked()) return 20;
        Registry protectedRegistry(productionPath(), {}, Hive::LocalMachine, true);
        result = protectedRegistry.apply(true);
    }
    if (!result.ok) {
        QMessageBox::warning(nullptr, "HKLM", result.message + "\n\n" +
            t("Propriétaire et permissions conservés. Si le privilège requis n'est pas attribué à ce compte, Windows refuse aussi la tentative avancée.",
              "Ownership and permissions preserved. If the required privilege is not assigned to this account, Windows also refuses the advanced attempt."));
        return 30;
    }
    QMessageBox::information(nullptr, "HKLM", t("Configuration HKLM vérifiée. Explorer n'a pas été redémarré.\nLes autres sessions devront être actualisées séparément.",
        "HKLM configuration verified. Explorer was not restarted.\nOther sessions must be refreshed separately."));
    return result.message == "changed" ? 0 : 10;
}
}
