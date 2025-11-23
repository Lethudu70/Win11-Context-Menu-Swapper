import sys
import subprocess
from PyQt5.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QPushButton, QLabel, QTextEdit, QHBoxLayout, QGraphicsDropShadowEffect
)
from PyQt5.QtCore import Qt, QTimer, QPoint, QRectF
from PyQt5.QtGui import QPalette, QColor, QLinearGradient, QBrush, QRegion, QPainterPath

LEGACY_KEY = r"HKCU\Software\Classes\CLSID\{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}"


# ---- Registry functions ----
def enable_legacy():
    subprocess.run([
        "reg", "add",
        LEGACY_KEY + r"\InprocServer32",
        "/f", "/ve"
    ], check=True)


def disable_legacy():
    subprocess.run([
        "reg", "delete",
        LEGACY_KEY,
        "/f"
    ], check=False)


def restart_explorer():
    subprocess.run(["taskkill", "/IM", "explorer.exe", "/F"])
    subprocess.Popen("explorer.exe")


# ---- Fake Console ----
class FakeConsole(QTextEdit):
    def __init__(self):
        super().__init__()
        self.setReadOnly(True)
        self.setStyleSheet("""
            QTextEdit {
                background-color: #04040A;
                color: #D9B8FF;
                border: 2px solid #7A32DB;
                padding: 10px;
                font-family: Consolas;
                font-size: 16px;
                border-radius: 12px;
            }
        """)

    def log(self, text):
        self.append(text)


# ---- Main Window ----
class ContextMenuSwitcher(QWidget):
    def __init__(self):
        super().__init__()

        # Remove Windows titlebar
        self.setWindowFlags(Qt.FramelessWindowHint)

        # Rounded window
        self.radius = 30
        self.resize(720, 530)
        self.dragPos = None

        self.apply_background()
        self.init_ui()

    # Rounded window
    def resizeEvent(self, event):
        path = QPainterPath()
        rect = QRectF(0, 0, self.width(), self.height())
        path.addRoundedRect(rect, self.radius, self.radius)
        region = QRegion(path.toFillPolygon().toPolygon())
        self.setMask(region)

    # Drag window
    def mousePressEvent(self, event):
        if event.button() == Qt.LeftButton:
            self.dragPos = event.globalPos()

    def mouseMoveEvent(self, event):
        if event.buttons() == Qt.LeftButton and self.dragPos:
            delta = event.globalPos() - self.dragPos
            self.move(self.x() + delta.x(), self.y() + delta.y())
            self.dragPos = event.globalPos()

    def apply_background(self):
        palette = QPalette()
        gradient = QLinearGradient(0, 0, 1, 1)
        gradient.setCoordinateMode(QLinearGradient.ObjectBoundingMode)
        gradient.setColorAt(0, QColor("#020818"))
        gradient.setColorAt(0.5, QColor("#0C1A5A"))
        gradient.setColorAt(1, QColor("#4B0C7E"))
        palette.setBrush(QPalette.Window, QBrush(gradient))
        self.setAutoFillBackground(True)
        self.setPalette(palette)

    def init_ui(self):
        main = QVBoxLayout()
        main.setSpacing(20)
        main.setContentsMargins(20, 20, 20, 20)

        # ---- Custom title bar ----
        title_bar = QHBoxLayout()
        title_bar.setSpacing(12)

        # Spacer left + title for centering
        title_layout = QHBoxLayout()
        title_layout.addStretch()
        title_label = QLabel("Windows Context Menu Switcher")
        title_label.setStyleSheet("color: white; font-size: 22px; font-weight: bold;")
        title_layout.addWidget(title_label)
        title_layout.addStretch()
        title_bar.addLayout(title_layout)

        # Base button style
        btn_round = """
            QPushButton {
                border-radius: 12px;
                min-width: 26px;
                max-width: 26px;
                min-height: 26px;
                max-height: 26px;
                font-weight: bold;
                font-size: 14px;
                background-color: transparent;
                color: white;
            }
        """

        # Close button (X) with LethOS hover glow
        btn_close = QPushButton("✕")
        btn_close.setStyleSheet(btn_round + """
            QPushButton:hover {
                background-color: rgba(180, 0, 255, 0.7);
                border: 1px solid #B077FF;
            }
        """)
        btn_close.clicked.connect(self.close)
        glow_effect = QGraphicsDropShadowEffect()
        glow_effect.setBlurRadius(12)
        glow_effect.setColor(QColor(186, 71, 255))
        glow_effect.setOffset(0)
        btn_close.setGraphicsEffect(glow_effect)

        # Maximize / Restore (◻) hover grey
        btn_resize = QPushButton("◻")
        btn_resize.setStyleSheet(btn_round + """
            QPushButton:hover {
                background-color: rgba(200, 200, 200, 0.3);
            }
        """)
        btn_resize.clicked.connect(self.toggle_resize)

        # Minimize (—) hover grey
        btn_min = QPushButton("—")
        btn_min.setStyleSheet(btn_round + """
            QPushButton:hover {
                background-color: rgba(200, 200, 200, 0.3);
            }
        """)
        btn_min.clicked.connect(self.showMinimized)

        # Add buttons to title bar (right)
        title_bar.addWidget(btn_min)
        title_bar.addWidget(btn_resize)
        title_bar.addWidget(btn_close)

        # ---- Main Buttons ----
        btn_style = """
            QPushButton {
                background-color: rgba(60, 0, 120, 0.45);
                color: #F0DFFF;
                border: 2px solid #B077FF;
                border-radius: 16px;
                padding: 14px;
                font-size: 18px;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: rgba(140, 0, 255, 0.6);
            }
        """

        btn_legacy = QPushButton("Enable Legacy Context Menu (Windows 10 Style)")
        btn_modern = QPushButton("Enable Modern Context Menu (Windows 11 Style)")

        btn_legacy.setStyleSheet(btn_style)
        btn_modern.setStyleSheet(btn_style)

        btn_legacy.clicked.connect(self.enable_legacy_action)
        btn_modern.clicked.connect(self.enable_modern_action)

        # ---- Console ----
        self.console = FakeConsole()
        self.console.setFixedHeight(260)

        main.addLayout(title_bar)
        main.addWidget(btn_legacy)
        main.addWidget(btn_modern)
        main.addWidget(self.console)

        self.setLayout(main)

    def toggle_resize(self):
        if self.isMaximized():
            self.showNormal()
        else:
            self.showMaximized()

    def fake_logs(self, steps, callback):
        self.console.clear()

        def next_step(i=0):
            if i < len(steps):
                self.console.log(steps[i])
                QTimer.singleShot(650, lambda: next_step(i + 1))
            else:
                callback()

        next_step()

    def enable_legacy_action(self):
        logs = [
            "> Applying system configuration...",
            "> Accessing registry (HKCU)...",
            "> Enabling Legacy Context Menu override...",
            "> Writing configuration...",
            "> Restarting Windows Explorer...",
            "> Finalizing operation..."
        ]
        self.fake_logs(logs, lambda: (
            enable_legacy(),
            restart_explorer(),
            self.console.log("\n✔ Legacy Context Menu Activated!")
        ))

    def enable_modern_action(self):
        logs = [
            "> Applying system configuration...",
            "> Accessing registry (HKCU)...",
            "> Removing legacy override...",
            "> Restoring modern context menu...",
            "> Restarting Windows Explorer...",
            "> Finalizing operation..."
        ]
        self.fake_logs(logs, lambda: (
            disable_legacy(),
            restart_explorer(),
            self.console.log("\n✔ Modern Context Menu Restored!")
        ))


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = ContextMenuSwitcher()
    window.show()
    sys.exit(app.exec_())
