#include "window.h"
#include "machine.h"
#include "cache.h"
#include <windows.h>
#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QCloseEvent>
#include <QAbstractButton>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSaveFile>
#include <QSettings>
#include <QSignalBlocker>
#include <QStyleHints>
#include <QStyle>
#include <QSizeGrip>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QWindow>

namespace cms {
namespace {
class Caption : public QWidget {
public:
    explicit Caption(QWidget *parent) : QWidget(parent) {
        setObjectName("caption");
    }
protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() != Qt::LeftButton) { QWidget::mousePressEvent(event); return; }
        auto *top = window();
        if (top->windowHandle() && top->windowHandle()->startSystemMove()) {
            event->accept(); return;
        }
        // Fallback for platforms without native system move support.
        dragging_ = true;
        offset_ = event->globalPosition().toPoint() - top->frameGeometry().topLeft();
        event->accept();
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        if (dragging_ && (event->buttons() & Qt::LeftButton)) {
            window()->move(event->globalPosition().toPoint() - offset_);
            event->accept();
        } else QWidget::mouseMoveEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        dragging_ = false;
        QWidget::mouseReleaseEvent(event);
    }
    void mouseDoubleClickEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            if (window()->isMaximized()) window()->showNormal();
            else window()->showMaximized();
            event->accept();
        } else QWidget::mouseDoubleClickEvent(event);
    }
private:
    bool dragging_ = false;
    QPoint offset_;
};
bool confirm(QWidget *parent, const QString &title, const QString &text, bool french) {
    QMessageBox box(QMessageBox::Question, title, text, QMessageBox::Yes | QMessageBox::No, parent);
    box.button(QMessageBox::Yes)->setText(french ? "Continuer" : "Continue");
    box.button(QMessageBox::No)->setText(french ? "Annuler" : "Cancel");
    box.setDefaultButton(QMessageBox::No);
    return box.exec() == QMessageBox::Yes;
}
DWORD shellPid() {
    DWORD pid = 0;
    const auto shell = GetShellWindow();
    if (shell) GetWindowThreadProcessId(shell, &pid);
    return pid;
}
QString shellIdentity() {
    const auto pid = shellPid();
    if (!pid) return {};
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    FILETIME created{}, exited{}, kernel{}, user{};
    bool ok = process && GetProcessTimes(process, &created, &exited, &kernel, &user);
    if (process) CloseHandle(process);
    return ok ? QStringLiteral("%1:%2:%3").arg(pid).arg(created.dwHighDateTime).arg(created.dwLowDateTime) : QString{};
}
}
QString Window::t(const char *fr, const char *en) const { return QString::fromUtf8(french_ ? fr : en); }
Registry &Window::activeRegistry() { return experimental_ && experimental_->isChecked() ? machineRegistry_ : registry_; }
Window::Window(Registry &registry) : registry_(registry) {
    QSettings settings;
    french_ = settings.value("language", "fr").toString() == "fr";
    theme_ = settings.value("theme", "leth").toString();
    pending_ = settings.value("pending", false).toBool();
    if (pending_ && !shellIdentity().isEmpty() &&
        settings.value("shellIdentity").toString() != shellIdentity()) {
        pending_ = false;
        settings.setValue("pending", false);
    }
    setWindowTitle("Context Menu Swapper v4.1 — Leth Edition");
    setWindowIcon(QIcon(":/app.ico"));
    setObjectName("window");
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowMinimizeButtonHint);
    setAttribute(Qt::WA_TranslucentBackground);
    resize(840, 850); setMinimumSize(760, 740);
    outer_ = new QVBoxLayout(this); outer_->setContentsMargins(14, 14, 14, 14);
    surface_ = new QWidget(this); surface_->setObjectName("surface");
    surface_->setAttribute(Qt::WA_StyledBackground);
    auto *shadow = new QGraphicsDropShadowEffect(surface_);
    shadow->setBlurRadius(24); shadow->setOffset(0, 4); shadow->setColor(QColor(0, 0, 0, 120));
    surface_->setGraphicsEffect(shadow);
    outer_->addWidget(surface_);
    auto *layout = new QVBoxLayout(surface_);
    layout->setContentsMargins(26, 20, 26, 20); layout->setSpacing(14);
    auto *caption = new Caption(surface_);
    auto *header = new QHBoxLayout(caption); header->setContentsMargins(0, 0, 0, 0); header->setSpacing(12);
    auto *icon = new QLabel; icon->setPixmap(windowIcon().pixmap(46, 46));
    icon->setAttribute(Qt::WA_TransparentForMouseEvents);
    header->addWidget(icon);
    auto *titles = new QVBoxLayout;
    auto *title = new QLabel("CONTEXT MENU SWAPPER"); title->setObjectName("title");
    title->setAttribute(Qt::WA_TransparentForMouseEvents);
    subtitle_ = new QLabel; subtitle_->setObjectName("muted");
    subtitle_->setAttribute(Qt::WA_TransparentForMouseEvents);
    titles->addWidget(title); titles->addWidget(subtitle_);
    header->addLayout(titles); header->addStretch();
    language_ = new QComboBox; language_->addItems({"Français", "English"}); language_->setCurrentIndex(french_ ? 0 : 1);
    minimize_ = new QPushButton(QString::fromUtf8("−")); minimize_->setObjectName("minimizeButton");
    close_ = new QPushButton(QString::fromUtf8("×")); close_->setObjectName("closeButton");
    for (auto *b : {minimize_, close_}) { b->setFixedSize(38, 34); b->setCursor(Qt::PointingHandCursor); header->addWidget(b); }
    connect(minimize_, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(close_, &QPushButton::clicked, this, &QWidget::close);
    layout->addWidget(caption);
    system_ = new QLabel(systemDescription()); system_->setObjectName("systemInfo");
    system_->setWordWrap(true); system_->setTextInteractionFlags(Qt::TextSelectableByMouse); layout->addWidget(system_);
    state_ = new QLabel; state_->setObjectName("state"); state_->setWordWrap(true); layout->addWidget(state_);
    shell_ = new QLabel; shell_->setObjectName("muted"); shell_->setWordWrap(true); layout->addWidget(shell_);
    experimental_ = new QCheckBox; experimental_->setChecked(false); layout->addWidget(experimental_);
    experimentalWarning_ = new QLabel; experimentalWarning_->setObjectName("experimentalWarning");
    experimentalWarning_->setWordWrap(true); experimentalWarning_->hide(); layout->addWidget(experimentalWarning_);
    auto *cards = new QHBoxLayout;
    classic_ = new QPushButton; modern_ = new QPushButton;
    for (auto *button : {classic_, modern_}) { button->setCheckable(true); button->setMinimumHeight(105); cards->addWidget(button); }
    layout->addLayout(cards);
    connect(classic_, &QPushButton::clicked, this, [this] { selected_ = "classic"; refresh(); });
    connect(modern_, &QPushButton::clicked, this, [this] { selected_ = "modern"; refresh(); });
    hint_ = new QLabel; hint_->setWordWrap(true); hint_->setObjectName("muted"); layout->addWidget(hint_);
    apply_ = new QPushButton; apply_->setObjectName("primary"); apply_->setMinimumHeight(44); layout->addWidget(apply_);
    auto *actions = new QHBoxLayout;
    undo_ = new QPushButton; restart_ = new QPushButton; refresh_ = new QPushButton;
    actions->addWidget(undo_); actions->addWidget(restart_); actions->addWidget(refresh_); layout->addLayout(actions);
    log_ = new QPlainTextEdit; log_->setReadOnly(true); log_->setMaximumBlockCount(1000);
    log_->setMinimumHeight(125); layout->addWidget(log_, 1);
    auto *bottom = new QHBoxLayout;
    themes_ = new QComboBox; themes_->addItems({"Leth · dark", "System"}); themes_->setCurrentIndex(theme_ == "leth" ? 0 : 1);
    export_ = new QPushButton;
    cleanup_ = new QPushButton;
    bottom->addWidget(language_); bottom->addWidget(themes_); bottom->addStretch(); bottom->addWidget(cleanup_); bottom->addWidget(export_);
    auto *grip = new QSizeGrip(surface_); grip->setFixedSize(18, 18); bottom->addWidget(grip, 0, Qt::AlignBottom);
    layout->addLayout(bottom);
    footer_ = new QLabel; footer_->setObjectName("muted"); footer_->setWordWrap(true); layout->addWidget(footer_);
    connect(apply_, &QPushButton::clicked, this, &Window::apply);
    connect(undo_, &QPushButton::clicked, this, &Window::undo);
    connect(restart_, &QPushButton::clicked, this, &Window::restartExplorer);
    connect(refresh_, &QPushButton::clicked, this, &Window::refresh);
    connect(export_, &QPushButton::clicked, this, &Window::exportDiagnostic);
    connect(cleanup_, &QPushButton::clicked, this, &Window::cleanCache);
    connect(experimental_, &QCheckBox::toggled, this, [this](bool enabled) {
        experimentalWarning_->setVisible(enabled); selected_ = "classic";
        if (enabled) {
            standardSize_ = size(); setMinimumHeight(940); resize(width(), qMax(height(), 940));
        } else {
            setMinimumHeight(740);
            if (standardSize_.isValid() && !isMaximized()) resize(standardSize_);
        }
        texts(); refresh();
    });
    connect(language_, &QComboBox::currentIndexChanged, this, [this](int i) {
        french_ = i == 0; QSettings().setValue("language", french_ ? "fr" : "en"); texts(); refresh();
    });
    connect(themes_, &QComboBox::currentIndexChanged, this, [this](int i) {
        theme_ = i == 0 ? "leth" : "system"; QSettings().setValue("theme", theme_); applyTheme();
    });
    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, [this] { if (theme_ == "system") applyTheme(); });
    poll_ = new QTimer(this); poll_->setInterval(2500);
    connect(poll_, &QTimer::timeout, this, [this] {
        if (busy_) return;
        QSettings s;
        const auto identity = shellIdentity();
        if (pending_ && !identity.isEmpty() && s.value("shellIdentity").toString() != identity) {
            pending_ = false; s.setValue("pending", false);
        }
        refresh();
    });
    poll_->start(); texts(); applyTheme(); refresh();
    log(t("Prêt. Aucune modification effectuée au démarrage.", "Ready. No registry changes on startup."));
    if (registry_.backup().value("status").toString() == "prepared")
        log(t("Opération interrompue détectée : utiliser Annuler avant de continuer.",
              "Interrupted operation detected: use Undo before continuing."));
}
void Window::texts() {
    const bool machine = activeRegistry().machine();
    subtitle_->setText(t("v4.1 · Leth Edition · changement réversible", "v4.1 · Leth Edition · reversible changes"));
    minimize_->setToolTip(t("Réduire", "Minimize")); minimize_->setAccessibleName(minimize_->toolTip());
    close_->setToolTip(t("Fermer", "Close")); close_->setAccessibleName(close_->toolTip());
    system_->setToolTip(t("Système Windows détecté sur cet ordinateur", "Windows system detected on this computer"));
    classic_->setText(t("MENU CLASSIQUE\nAnciennes options directement accessibles", "CLASSIC MENU\nDirect access to legacy options"));
    modern_->setText(machine ? t("RESTAURER HKLM\nConfiguration système sauvegardée", "RESTORE HKLM\nSaved system configuration") :
        t("MENU WINDOWS 11\nComportement Windows par défaut", "WINDOWS 11 MENU\nDefault Windows behavior"));
    experimental_->setText(t("Mode expérimental HKLM — non recommandé aux utilisateurs standard", "Experimental HKLM mode — not recommended for standard users"));
    experimentalWarning_->setText(t("EXPÉRIMENTAL · Tous les comptes peuvent être affectés ; HKCU peut rester prioritaire. Appliquer/restaurer ouvre l'UAC. Sur une clé protégée, une confirmation avancée permet de tenter le privilège de restauration Windows. Propriétaire et permissions conservés.",
        "EXPERIMENTAL · All accounts may be affected; HKCU may take precedence. Applying/restoring opens UAC. For a protected key, advanced confirmation allows a Windows restore-privilege attempt. Ownership and permissions preserved."));
    hint_->setText(machine ? t("Cible : HKLM (vue 64 bits). Sauvegarde système séparée. HKCU reste inchangé.",
        "Target: HKLM (64-bit view). Separate system backup. HKCU remains unchanged.") :
        t("Modification limitée à votre compte (HKCU). Sauvegarde automatique avant application.",
          "Change limited to your account (HKCU). Automatic backup before applying."));
    apply_->setText(machine ? t("Appliquer en HKLM… (UAC)", "Apply to HKLM… (UAC)") : t("Appliquer la sélection", "Apply selection"));
    undo_->setText(machine ? t("Annuler HKLM… (UAC)", "Undo HKLM… (UAC)") : t("Annuler le changement", "Undo change"));
    restart_->setText(t("Redémarrer Explorer…", "Restart Explorer…"));
    refresh_->setText(t("Actualiser", "Refresh"));
    export_->setText(t("Exporter le diagnostic…", "Export diagnostics…"));
    cleanup_->setText(t("Nettoyer JSON…", "Clean JSON…"));
    footer_->setText(t("La configuration du registre est vérifiée. L'effet visuel dépend de votre build Windows ; testez le clic droit après redémarrage d'Explorer.",
                      "Registry configuration is verified. Visual behavior depends on your Windows build; test right-click after restarting Explorer."));
}
void Window::refresh() {
    auto &registry = activeRegistry();
    const auto current = registry.read();
    const auto mode = current.mode();
    auto label = mode == "classic" ? t("Classique configuré", "Classic configured") :
        mode == "modern" ? t("Windows par défaut", "Windows default") :
        mode == "custom" ? t("Configuration personnalisée", "Custom configuration") : t("Lecture impossible", "Cannot read configuration");
    if (registry.machine() && mode == "custom") label = t("Configuration système existante", "Existing system configuration");
    if (registry.machine() && mode == "modern") label = t("Aucun override HKLM", "No HKLM override");
    state_->setText("CONFIGURATION " + registry.hiveName() + "  ·  " + label);
    shell_->setText(pending_ ? t("● Changement en attente : redémarrez Explorer maintenant ou plus tard.",
                                "● Pending change: restart Explorer now or later.") :
                              t("Aucun redémarrage en attente connu de l'application.", "No restart pending according to this app."));
    classic_->setChecked(selected_ == "classic"); modern_->setChecked(selected_ == "modern");
    const bool interrupted = registry.backup().value("status").toString() == "prepared";
    const bool available = registry.machine() && selected_ == "modern" ? registry.canUndo() : !interrupted && mode != selected_;
    apply_->setEnabled(!busy_ && (current.ok || registry.machine()) && available);
    undo_->setEnabled(!busy_ && registry.canUndo());
    experimental_->setEnabled(!busy_);
    for (auto *b : {classic_, modern_, restart_, refresh_, export_, cleanup_, close_}) b->setEnabled(!busy_);
    if (!current.ok) state_->setToolTip(current.error);
}
void Window::log(const QString &message) { log_->appendPlainText(QDateTime::currentDateTime().toString("HH:mm:ss") + "  " + message); }
void Window::setBusy(bool value) { busy_ = value; refresh(); }
void Window::markPending() {
    pending_ = true; changedAt_ = QDateTime::currentDateTimeUtc();
    QSettings s; s.setValue("pending", true); s.setValue("shellIdentity", shellIdentity());
}
void Window::apply() {
    if (busy_) return;
    if (activeRegistry().machine()) { machineAction(selected_ == "classic" ? "classic" : "undo"); return; }
    const bool classic = selected_ == "classic";
    QString description = classic ? t("Configurer le menu classique pour votre compte ?", "Configure the classic menu for your account?") :
        t("Rétablir Windows par défaut ? Les deux clés CLSID ciblées seront sauvegardées puis retirées.",
          "Restore Windows defaults? Both targeted CLSID keys will be backed up and removed.");
    description += "\n\n" + registry_.targets() + "\n\n" +
        t("Explorer ne sera pas redémarré automatiquement.", "Explorer will not restart automatically.");
    if (!confirm(this, t("Appliquer", "Apply"), description, french_)) return;
    setBusy(true);
    // Run after Qt has painted the disabled controls. Registry operations are bounded to this small subtree.
    QTimer::singleShot(0, this, [this, classic] {
        auto result = registry_.apply(classic);
        if (result.ok && result.message == "changed") markPending();
        log(result.ok ? t("Configuration vérifiée. Sauvegarde : ", "Configuration verified. Backup: ") + registry_.backupPath() : result.message);
        if (!result.ok) QMessageBox::warning(this, t("Échec de l'application", "Apply failed"), result.message);
        setBusy(false);
    });
}
void Window::undo() {
    if (busy_) return;
    if (activeRegistry().machine()) { machineAction("undo"); return; }
    if (!confirm(this, t("Annuler", "Undo"),
        t("Restaurer la configuration sauvegardée avant le dernier changement ?", "Restore the configuration saved before the last change?"), french_)) return;
    setBusy(true);
    QTimer::singleShot(0, this, [this] {
        auto result = registry_.undo();
        if (result.ok && result.message == "changed") markPending();
        log(result.ok ? t("Configuration précédente restaurée et vérifiée.", "Previous configuration restored and verified.") : result.message);
        if (!result.ok) QMessageBox::warning(this, t("Restauration impossible", "Restore failed"), result.message);
        setBusy(false);
    });
}
void Window::machineAction(const QString &action) {
    if (busy_ || !activeRegistry().machine() || !validMachineAction(action)) return;
    setBusy(true);
    log(t("Demande d'autorisation UAC pour HKLM…", "Requesting UAC authorization for HKLM…"));
    const auto launch = launchMachineAction(action, french_, reinterpret_cast<void *>(winId()));
    if (!launch.process) {
        log(launch.error == ERROR_CANCELLED ? t("UAC annulé : aucune opération HKLM lancée.", "UAC canceled: no HKLM operation started.") :
            t("Impossible de lancer l'opération HKLM. Code Windows : ", "Cannot start HKLM operation. Windows code: ") + QString::number(launch.error));
        setBusy(false); return;
    }
    auto *watch = new QTimer(this); watch->setInterval(250);
    connect(watch, &QTimer::timeout, this, [this, watch, handle = static_cast<HANDLE>(launch.process)] {
        DWORD code = STILL_ACTIVE;
        if (!GetExitCodeProcess(handle, &code)) code = 30;
        if (code == STILL_ACTIVE) return;
        watch->stop(); watch->deleteLater(); CloseHandle(handle);
        if (code == 0) { markPending(); log(t("Opération HKLM vérifiée. Redémarrage des sessions à effectuer séparément.",
            "HKLM operation verified. Sessions must be refreshed separately.")); }
        else if (code == 10) log(t("HKLM : configuration déjà en place.", "HKLM: configuration already in place."));
        else if (code == 20) log(t("Confirmation HKLM annulée : aucune modification.", "HKLM confirmation canceled: no change."));
        else log(t("L'opération HKLM a échoué. Consultez le message du processus administrateur. Code : ",
                   "HKLM operation failed. See the administrator process message. Code: ") + QString::number(code));
        setBusy(false);
    }); watch->start();
}
void Window::restartExplorer() {
    if (busy_) return;
    const auto pid = shellPid(); DWORD shellSession = 0, ownSession = 0;
    if (!pid || !ProcessIdToSessionId(pid, &shellSession) ||
        !ProcessIdToSessionId(GetCurrentProcessId(), &ownSession) || ownSession != shellSession) {
        QMessageBox::warning(this, "Explorer", t("Explorer de cette session est introuvable. Déconnectez-vous puis reconnectez-vous pour appliquer le changement.",
            "Cannot find this session's Explorer. Sign out and back in to apply the change.")); return;
    }
    if (!confirm(this, "Explorer", t(
        "Redémarrer Explorer maintenant ?\nLes fenêtres de l'Explorateur seront fermées et la barre des tâches disparaîtra brièvement. Terminez vos copies de fichiers avant de continuer.",
        "Restart Explorer now?\nFile Explorer windows will close and the taskbar will briefly disappear. Finish file transfers before continuing."), french_)) return;
    const auto previousIdentity = shellIdentity();
    setBusy(true); log(t("Redémarrage d'Explorer…", "Restarting Explorer…"));
    auto *process = new QProcess(this);
    auto *timeout = new QTimer(process); timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, process, &QProcess::kill);
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) { log(process->errorString()); setBusy(false); process->deleteLater(); }
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
        [this, process, timeout, previousIdentity](int code, QProcess::ExitStatus status) {
        timeout->stop();
        const auto details = QString::fromLocal8Bit(process->readAllStandardError());
        process->deleteLater();
        if (code != 0 || status != QProcess::NormalExit) { log(t("Échec du redémarrage : ", "Restart failed: ") + details); setBusy(false); return; }
        const auto explorer = QDir::toNativeSeparators(qEnvironmentVariable("WINDIR", "C:/Windows") + "/explorer.exe");
        // Windows may restart the shell itself. Avoid launching a second Explorer window in that case.
        QTimer::singleShot(1000, this, [this, explorer, previousIdentity] {
            if (!shellPid()) QProcess::startDetached(explorer, {});
            auto *watch = new QTimer(this); watch->setInterval(500);
            auto *count = new int(0);
            connect(watch, &QTimer::timeout, this, [this, watch, count, previousIdentity] {
                ++*count;
                const auto identity = shellIdentity();
                if ((!identity.isEmpty() && identity != previousIdentity) || *count >= 20) {
                    const bool ok = !identity.isEmpty() && identity != previousIdentity;
                    if (ok) { pending_ = false; QSettings().setValue("pending", false); }
                    log(ok ? t("Explorer redémarré. Vous pouvez tester le clic droit.", "Explorer restarted. You can test right-click.") :
                             t("Redémarrage non confirmé. Déconnectez-vous puis reconnectez-vous si nécessaire.", "Restart not confirmed. Sign out and back in if needed."));
                    watch->stop(); watch->deleteLater(); delete count; setBusy(false);
                }
            }); watch->start();
        });
    });
    const auto taskkill = QDir::toNativeSeparators(qEnvironmentVariable("WINDIR", "C:/Windows") + "/System32/taskkill.exe");
    process->start(taskkill, {"/PID", QString::number(pid), "/F"}); timeout->start(10000);
}
void Window::applyTheme() {
    const bool dark = theme_ == "leth" || qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    const QString bg = dark ? "#0b1110" : "#f3f6f4", panel = dark ? "#131e19" : "#ffffff";
    const QString fg = dark ? "#e6f0eb" : "#15271d", muted = dark ? "#9eb1a5" : "#4c6456";
    const QString accent = dark ? "#66eea2" : "#12683a", border = dark ? "#304b3b" : "#b5cbbd";
    setStyleSheet(QString(
        "QWidget {background:%1;color:%3;font-family:'Segoe UI';font-size:10pt;}"
        "QWidget#window {background:transparent;}"
        "QWidget#surface {background:%1;border:1px solid %6;border-radius:22px;}"
        "QWidget#surface[maximized=\"true\"] {border-radius:0px;border:0px;}"
        "QWidget#caption {background:transparent;}"
        "QCheckBox {spacing:10px;}"
        "QCheckBox::indicator {width:18px;height:18px;border:2px solid %5;border-radius:4px;background:%2;}"
        "QCheckBox::indicator:checked {background:%5;image:url(:/checkmark.png);}"
        "QCheckBox::indicator:hover {border-width:3px;}"
        "QCheckBox::indicator:disabled {border-color:%4;}"
        "QLabel#title {font-size:17pt;font-weight:700;letter-spacing:1px;}"
        "QLabel#systemInfo {background:%2;color:%5;border:1px solid %6;border-radius:9px;padding:11px;font-weight:600;}"
        "QLabel#experimentalWarning {background:#302311;color:#ffda93;border:1px solid #b68b45;border-radius:9px;padding:11px;}"
        "QLabel#muted {color:%4;font-size:9pt;} QLabel#state {background:%2;border:1px solid %6;border-radius:12px;padding:18px;font-weight:600;font-size:12pt;}"
        "QPushButton,QComboBox {background:%2;border:1px solid %6;border-radius:9px;padding:10px;}"
        "QPushButton:hover {border-color:%5;} QPushButton:checked {border:2px solid %5;color:%5;}"
        "QPushButton#primary {background:%5;color:%1;font-weight:700;}"
        "QPushButton:disabled {color:%4;background:%1;border-color:%6;}"
        "QPushButton#primary:disabled {color:%4;background:%2;border:1px solid %6;}"
        "QPushButton:focus,QComboBox:focus {border:2px solid %5;}"
        "QPushButton#minimizeButton,QPushButton#closeButton {padding:0px;font-size:20pt;border:1px solid %6;background:%2;}"
        "QPushButton#minimizeButton:hover {background:%6;border-color:%5;}"
        "QPushButton#closeButton:hover {background:#b5233a;color:white;border-color:#e15c70;}"
        "QPushButton#closeButton:disabled {color:%4;background:%1;}"
        "QPlainTextEdit {background:%2;border:1px solid %6;border-radius:9px;padding:10px;font-family:Consolas;font-size:9pt;}"
        "QComboBox QAbstractItemView {background:%2;color:%3;selection-background-color:%6;}"
    ).arg(bg, panel, fg, muted, accent, border));
}
void Window::cleanCache() {
    if (busy_) return;
    QMessageBox box(QMessageBox::Question, t("Nettoyer les JSON", "Clean JSON files"),
        t("Supprimer les caches reconnus et les sauvegardes HKCU déjà annulées ?\n\nLes sauvegardes actives restent disponibles. Les JSON d'annulation illisibles sont archivés avant réinitialisation. Les fichiers JSON inconnus et la sauvegarde HKLM ne sont pas supprimés.\n\nCe nettoyage ne corrige pas un refus d'accès du registre Windows.",
          "Remove recognized caches and completed HKCU undo history?\n\nActive backups stay available. Unreadable undo JSON is archived before reset. Unknown JSON files and the HKLM backup are not deleted.\n\nThis cleanup does not fix Windows registry access denial."), QMessageBox::Yes | QMessageBox::Cancel, this);
    auto *reset = new QCheckBox(t("Réinitialiser aussi l'annulation HKCU active (archiver puis retirer).",
                                 "Also reset active HKCU undo history (archive, then remove)."));
    box.setCheckBox(reset);
    box.button(QMessageBox::Yes)->setText(t("Nettoyer", "Clean")); box.button(QMessageBox::Cancel)->setText(t("Annuler", "Cancel"));
    box.setDefaultButton(QMessageBox::Cancel);
    if (box.exec() != QMessageBox::Yes) return;
    setBusy(true);
    const auto result = cleanJsonCache(QFileInfo(registry_.backupPath()).absolutePath(), registry_, reset->isChecked());
    log(t("Nettoyage JSON : ", "JSON cleanup: ") + QString::number(result.removed) + t(" supprimé(s), ", " removed, ") +
        QString::number(result.archived) + t(" archivé(s), ", " archived, ") + QString::number(result.preserved) + t(" conservé(s).", " preserved."));
    if (!result.ok()) QMessageBox::warning(this, t("Nettoyage incomplet", "Cleanup incomplete"), result.errors.join('\n'));
    setBusy(false);
}
void Window::exportDiagnostic() {
    auto path = QFileDialog::getSaveFileName(this, t("Diagnostic", "Diagnostics"), "ContextMenuSwapper-diagnostic.txt", "Text (*.txt)");
    if (path.isEmpty()) return;
    auto &registry = activeRegistry();
    const auto s = registry.read();
    const QString data = "Context Menu Swapper 4.1.0\n" + systemDescription() +
        "\nUTC: " + QDateTime::currentDateTimeUtc().toString(Qt::ISODate) +
        "\nTargets:\n" + registry.targets() + "\nMode: " + s.mode() +
        "\nPending restart: " + (pending_ ? "yes" : "no") + "\nRead error: " + s.error +
        "\nExperimental: " + (registry.machine() ? "yes" : "no") +
        "\nBackup available: " + (registry.canUndo() ? "yes" : "no") +
        "\n\n" + log_->toPlainText() + "\n";
    QSaveFile file(path);
    const auto bytes = data.toUtf8();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        QMessageBox::warning(this, t("Export impossible", "Export failed"), file.errorString());
    else log(t("Diagnostic exporté : ", "Diagnostics exported: ") + path);
}
void Window::capture(const QString &path) {
    QTimer::singleShot(600, this, [this, path] {
        QDir().mkpath(QFileInfo(path).absolutePath());
        const auto image = grab().toImage();
        bool ok = image.save(path);
        QJsonArray checks;
        auto check = [&checks, &ok](const char *name, bool passed) {
            checks.append(QJsonObject{{"name", name}, {"passed", passed}}); ok = ok && passed;
        };
        check("HKLM experimental mode is off by default", !experimental_->isChecked() && activeRegistry().hiveName() == "HKCU");
        check("Qt frameless window enabled", windowFlags().testFlag(Qt::FramelessWindowHint));
        check("native Windows caption removed", (GetWindowLongPtrW(reinterpret_cast<HWND>(winId()), GWL_STYLE) & WS_CAPTION) == 0);
        check("transparent rounded corners", image.pixelColor(0, 0).alpha() < 32 &&
            image.pixelColor(image.width() - 1, 0).alpha() < 32 &&
            image.pixelColor(image.width() / 2, image.height() / 2).alpha() == 255);
        check("Windows version displayed", !system_->text().isEmpty() && system_->text().contains("build"));
        // Exercise alternate language/theme and selection without writing settings or the registry.
        QSignalBlocker blockLanguage(language_), blockTheme(themes_);
        french_ = false; theme_ = "system"; selected_ = "modern";
        language_->setCurrentIndex(1); themes_->setCurrentIndex(1); texts(); applyTheme(); refresh();
        qApp->processEvents();
        const QFileInfo info(path);
        ok = grab().save(info.dir().filePath(info.completeBaseName() + "-en-system.png")) && ok;
        const auto state = registry_.read();
        const bool expectedApply = state.ok && state.mode() != "modern" &&
            registry_.backup().value("status").toString() != "prepared";
        check("alternate language and selection", apply_->isEnabled() == expectedApply && modern_->isChecked() &&
            !classic_->isChecked() && apply_->text() == "Apply selection");
        minimize_->click(); qApp->processEvents();
        check("custom minimize button", isMinimized() && IsIconic(reinterpret_cast<HWND>(winId())));
        showNormal(); qApp->processEvents();
        check("window restores after minimizing", !isMinimized());
        const auto original = size(); resize(minimumSize()); qApp->processEvents();
        check("title controls fit minimum size", close_->mapTo(this, QPoint(close_->width(), 0)).x() < width() &&
            classic_->mapTo(this, QPoint(classic_->width(), 0)).x() <= modern_->mapTo(this, QPoint(0, 0)).x());
        ok = grab().save(info.dir().filePath(info.completeBaseName() + "-compact.png")) && ok;
        resize(original); qApp->processEvents();
        const auto userBefore = registry_.read();
        const auto machineBefore = machineRegistry_.read();
        const bool elevatedBefore = isProcessElevated();
        french_ = true; language_->setCurrentIndex(0);
        experimental_->setChecked(true); qApp->processEvents();
        check("HKLM scope has a visible experimental warning", activeRegistry().hiveName() == "HKLM" && experimentalWarning_->isVisible() &&
            experimentalWarning_->text().contains("EXPÉRIMENTAL") && apply_->text().contains("UAC") && undo_->text().contains("UAC"));
        check("HKLM modern action offers saved-state restoration", modern_->text().contains("RESTAURER HKLM"));
        check("experimental warning does not overlap bottom controls", language_->mapTo(this, QPoint(0, 0)).y() >=
            log_->mapTo(this, QPoint(0, log_->height())).y());
        ok = grab().save(info.dir().filePath(info.completeBaseName() + "-hklm.png")) && ok;
        experimental_->setChecked(false); qApp->processEvents();
        check("switching scope causes no registry writes or elevation", isProcessElevated() == elevatedBefore &&
            registry_.read().tree == userBefore.tree && registry_.read().secondary == userBefore.secondary &&
            machineRegistry_.read().tree == machineBefore.tree && machineRegistry_.read().secondary == machineBefore.secondary &&
            activeRegistry().hiveName() == "HKCU");
        close_->click(); qApp->processEvents();
        check("custom close button", !isVisible());
        const auto report = QJsonDocument(QJsonObject{{"passed", ok}, {"windows", system_->text()},
            {"productionMenuModified", false}, {"checks", checks}}).toJson(QJsonDocument::Indented);
        QSaveFile file(info.dir().filePath(info.completeBaseName() + "-ui-checks.json"));
        ok = file.open(QIODevice::WriteOnly) && file.write(report) == report.size() && file.commit() && ok;
        qApp->exit(ok ? 0 : 1);
    });
}
void Window::closeEvent(QCloseEvent *event) {
    if (busy_) event->ignore();
    else event->accept();
}
void Window::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (!outer_ || !surface_) return;
    const bool maximized = isMaximized();
    const int margin = maximized ? 0 : 14;
    outer_->setContentsMargins(margin, margin, margin, margin);
    if (surface_->property("maximized").toBool() != maximized) {
        surface_->setProperty("maximized", maximized);
        surface_->style()->unpolish(surface_); surface_->style()->polish(surface_);
    }
}
}
