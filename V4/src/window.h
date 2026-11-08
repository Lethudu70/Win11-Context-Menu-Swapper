#pragma once
#include "registry.h"
#include <QWidget>
#include <QDateTime>
class QLabel;
class QPushButton;
class QComboBox;
class QPlainTextEdit;
class QTimer;
class QCloseEvent;
class QResizeEvent;
class QVBoxLayout;
class QCheckBox;
namespace cms {
class Window : public QWidget {
public:
    explicit Window(Registry &registry);
    void capture(const QString &path);
protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
private:
    Registry &registry_;
    Registry machineRegistry_{productionPath(), {}, Hive::LocalMachine};
    Registry &activeRegistry();
    QCheckBox *experimental_ = nullptr;
    QLabel *experimentalWarning_;
    QSize standardSize_;
    bool busy_ = false, pending_ = false, french_ = true;
    QString selected_ = "classic", theme_;
    QDateTime changedAt_;
    QLabel *subtitle_, *state_, *shell_, *system_, *hint_, *footer_;
    QPushButton *classic_, *modern_, *apply_, *undo_, *restart_, *refresh_, *export_;
    QPushButton *cleanup_;
    QPushButton *minimize_, *close_;
    QWidget *surface_ = nullptr;
    QVBoxLayout *outer_ = nullptr;
    QComboBox *language_, *themes_;
    QPlainTextEdit *log_;
    QTimer *poll_;
    QString t(const char *fr, const char *en) const;
    void texts();
    void refresh();
    void log(const QString &message);
    void apply();
    void undo();
    void machineAction(const QString &action);
    void restartExplorer();
    void setBusy(bool value);
    void markPending();
    void applyTheme();
    void exportDiagnostic();
    void cleanCache();
};
}
