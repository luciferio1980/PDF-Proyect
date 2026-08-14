#include "ui/Theme.h"

#include <QApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>

namespace pdfforge::ui {

const Theme& theme() {
    static const Theme t;
    return t;
}

void applyApplicationTheme(QApplication& app) {
    if (QStyle* fusion = QStyleFactory::create(QStringLiteral("Fusion"))) {
        app.setStyle(fusion);
    }
    const auto& t = theme();
    QPalette pal;
    pal.setColor(QPalette::Window, t.workspace);
    pal.setColor(QPalette::WindowText, t.text);
    pal.setColor(QPalette::Base, t.panel);
    pal.setColor(QPalette::AlternateBase, t.panelAlt);
    pal.setColor(QPalette::ToolTipBase, t.panelAlt);
    pal.setColor(QPalette::ToolTipText, t.text);
    pal.setColor(QPalette::Text, t.text);
    pal.setColor(QPalette::Button, t.panelAlt);
    pal.setColor(QPalette::ButtonText, t.text);
    pal.setColor(QPalette::Highlight, t.copper);
    pal.setColor(QPalette::HighlightedText, QColor(0xFF, 0xF8, 0xF0));
    pal.setColor(QPalette::PlaceholderText, t.muted);
    app.setPalette(pal);
    app.setStyleSheet(styleSheet());
}

QString styleSheet() {
    return QStringLiteral(R"(
        QMainWindow, QDialog, QWidget {
            background: #1A1D21;
            color: #E8E4DC;
            font-family: "Segoe UI", "Noto Sans", "DejaVu Sans", sans-serif;
            font-size: 13px;
        }
        QToolBar {
            background: #22262C;
            border: none;
            padding: 6px 10px;
            spacing: 6px;
        }
        QToolButton, QPushButton {
            background: #2A3038;
            color: #E8E4DC;
            border: 1px solid #3A414A;
            border-radius: 4px;
            padding: 5px 10px;
        }
        QToolButton:hover, QPushButton:hover {
            border-color: #C45C26;
            color: #FFF4EC;
        }
        QLineEdit, QSpinBox, QComboBox {
            background: #16191D;
            border: 1px solid #3A414A;
            border-radius: 4px;
            padding: 4px 8px;
            color: #E8E4DC;
            selection-background-color: #C45C26;
        }
        QDockWidget {
            titlebar-close-icon: none;
            color: #E8E4DC;
        }
        QDockWidget::title {
            background: #22262C;
            padding: 6px 10px;
        }
        QStatusBar {
            background: #15181C;
            color: #9A9488;
        }
        QListWidget {
            background: #1A1D21;
            border: none;
            outline: none;
        }
        QListWidget::item:selected {
            background: #2E241C;
            color: #F4E6DC;
        }
        QScrollBar:vertical {
            background: #1A1D21;
            width: 10px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #3A414A;
            min-height: 24px;
            border-radius: 4px;
        }
        QFormLayout, QGroupBox {
            color: #E8E4DC;
        }
        QGroupBox {
            border: 1px solid #3A414A;
            border-radius: 4px;
            margin-top: 12px;
            padding-top: 8px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 4px;
            color: #C45C26;
        }
        QPlainTextEdit, QTextEdit {
            background: #16191D;
            border: 1px solid #3A414A;
            color: #E8E4DC;
        }
        QCheckBox {
            color: #E8E4DC;
        }
        QMenu::item:selected {
            background: #C45C26;
        }
    )");
}

}  // namespace pdfforge::ui
