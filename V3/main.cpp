#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QProcess>
#include <QTimer>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>

// ---- Logique Système ----
void runCommand(QString cmd, QStringList args) {
    QProcess::execute(cmd, args);
}

void enableLegacy() {
    QString key = "HKCU\\Software\\Classes\\CLSID\\{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}\\InprocServer32";
    runCommand("reg", {"add", key, "/f", "/ve"});
}

void disableLegacy() {
    QString key = "HKCU\\Software\\Classes\\CLSID\\{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}";
    runCommand("reg", {"delete", key, "/f"});
}

void restartExplorer() {
    runCommand("taskkill", {"/IM", "explorer.exe", "/F"});
    QProcess::startDetached("explorer.exe");
}

// ---- Classe Principale ----
class ContextMenuSwitcher : public QWidget {
    Q_OBJECT // IMPORTANT: Laisser cette ligne ici

public:
    ContextMenuSwitcher(QWidget *parent = nullptr) : QWidget(parent) {
        // Mode Frameless + Transparent pour les arrondis
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_TranslucentBackground);
        
        resize(720, 530);
        initUI();
    }

protected:
    // Dessin du fond Matrix / LethOS
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        
        // Dégradé Noir vers Bleu très sombre
        QLinearGradient gradient(0, 0, 0, height());
        gradient.setColorAt(0, QColor("#000000"));
        gradient.setColorAt(1, QColor("#0a0a15"));
        
        QPainterPath path;
        path.addRoundedRect(rect(), 30, 30);
        
        painter.fillPath(path, gradient);
        
        // Bordure verte néon (Matrix)
        painter.setPen(QPen(QColor("#00FF41"), 2));
        painter.drawPath(path);
    }

    // Gestion du déplacement de la fenêtre
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) dragPos = event->globalPosition().toPoint();
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        if (event->buttons() & Qt::LeftButton) {
            move(pos() + (event->globalPosition().toPoint() - dragPos));
            dragPos = event->globalPosition().toPoint();
        }
    }

private:
    QPoint dragPos;
    QTextEdit *console;

    void initUI() {
        auto *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(30, 20, 30, 30);
        mainLayout->setSpacing(15);

        // Barre de titre
        auto *titleLayout = new QHBoxLayout();
        auto *titleLabel = new QLabel("CONTEXT MENU SWAPPER V3");
        titleLabel->setStyleSheet("color: #00FF41; font-size: 18px; font-weight: bold; font-family: 'Consolas';");
        
        auto *btnClose = new QPushButton("✕");
        btnClose->setFixedSize(30, 30);
        btnClose->setStyleSheet(
            "QPushButton { background: #1a0000; color: #ff0000; border: 1px solid #ff0000; border-radius: 15px; font-weight: bold; }"
            "QPushButton:hover { background: #ff0000; color: black; }"
        );
        connect(btnClose, &QPushButton::clicked, this, &QWidget::close);

        titleLayout->addWidget(titleLabel);
        titleLayout->addStretch();
        titleLayout->addWidget(btnClose);

        // Style des boutons Matrix
        QString btnStyle = 
            "QPushButton { "
            "  background-color: #050505; color: white; border: 1px solid #333; "
            "  border-radius: 12px; padding: 15px; font-size: 14px; font-weight: bold; font-family: 'Segoe UI'; "
            "} "
            "QPushButton:hover { border: 1px solid #00FF41; color: #00FF41; background-color: #0a0a0a; }";

        auto *btnLegacy = new QPushButton("ENABLE WINDOWS 10 MENU (LEGACY)");
        auto *btnModern = new QPushButton("ENABLE WINDOWS 11 MENU (MODERN)");
        btnLegacy->setStyleSheet(btnStyle);
        btnModern->setStyleSheet(btnStyle);

        // Console Vert Matrix
        console = new QTextEdit();
        console->setReadOnly(true);
        console->setFixedHeight(240);
        console->setStyleSheet(
            "background-color: #000; color: #00FF41; border: 1px solid #00FF41; "
            "padding: 10px; font-family: 'Consolas'; font-size: 13px; border-radius: 8px;"
        );

        mainLayout->addLayout(titleLayout);
        mainLayout->addWidget(btnLegacy);
        mainLayout->addWidget(btnModern);
        mainLayout->addWidget(console);

        // Connecter les actions
        connect(btnLegacy, &QPushButton::clicked, [this]() { triggerUpdate(true); });
        connect(btnModern, &QPushButton::clicked, [this]() { triggerUpdate(false); });
    }

    void triggerUpdate(bool isLegacy) {
        console->clear();
        console->append(">> [SYSTEM] ACCESSING KERNEL REGISTRY...");
        
        QTimer::singleShot(600, [=]() {
            if(isLegacy) {
                console->append(">> [REGEDIT] INJECTING CLSID OVERRIDE...");
                enableLegacy();
            } else {
                console->append(">> [REGEDIT] REMOVING OVERRIDE KEY...");
                disableLegacy();
            }
            
            QTimer::singleShot(600, [=]() {
                console->append(">> [SHELL] RESTARTING EXPLORER.EXE...");
                restartExplorer();
                console->append("\n>> SUCCESS: SYSTEM REFRESHED.");
            });
        });
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    ContextMenuSwitcher w;
    w.show();
    return app.exec();
}

#include "main.moc"