#include "registry.h"
#include "window.h"
#include "machine.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QIcon>
#include <QLockFile>
#include <QMessageBox>
#include <QStandardPaths>
#include <QSettings>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("LethTools");
    QCoreApplication::setApplicationName("ContextMenuSwapperV4");
    QCoreApplication::setApplicationVersion("4.0.0");
    // File-based settings; no settings writes to HKCU.
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QCommandLineParser parser;
    parser.setApplicationDescription("Context Menu Swapper V4 — Leth Edition");
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"self-test", "Test registry round trips in an isolated temporary HKCU key."});
    parser.addOption({"report", "Self-test report path.", "path"});
    parser.addOption({"preview", "Save a screenshot and exit. No menu changes.", "path"});
    parser.addOption({"machine-action", "Internal elevated HKLM helper: classic or undo.", "action"});
    parser.addOption({"machine-language", "Language for the elevated helper: fr or en.", "language", "fr"});
    parser.process(app);
    if (parser.isSet("self-test")) return cms::selfTest(parser.value("report"));
    if (parser.isSet("machine-action")) return cms::runMachineAction(parser.value("machine-action"), parser.value("machine-language") != "en");
    const auto directory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!QDir().mkpath(directory)) return 1;
    QLockFile lock(directory + "/application.lock");
    if (!lock.tryLock(0)) {
        QMessageBox::information(nullptr, "Context Menu Swapper", "Une instance est déjà ouverte / An instance is already running.");
        return 1;
    }
    cms::Registry registry(cms::productionPath(), directory + "/last-change.json");
    cms::Window window(registry); window.show();
    if (parser.isSet("preview")) window.capture(parser.value("preview"));
    return app.exec();
}
