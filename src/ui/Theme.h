#pragma once

#include <QColor>
#include <QPalette>
#include <QString>

class QApplication;

namespace pdfforge::ui {

struct Theme {
    QColor workspace{0x1A, 0x1D, 0x21};
    QColor panel{0x22, 0x26, 0x2C};
    QColor panelAlt{0x2A, 0x30, 0x38};
    QColor paper{0xF4, 0xF1, 0xEA};
    QColor ink{0x1B, 0x18, 0x14};
    QColor copper{0xC4, 0x5C, 0x26};
    QColor copperSoft{0xC4, 0x5C, 0x26, 48};
    QColor text{0xE8, 0xE4, 0xDC};
    QColor muted{0x9A, 0x94, 0x88};
    QColor danger{0xC4, 0x3C, 0x2B};
};

const Theme& theme();
void applyApplicationTheme(QApplication& app);
QString styleSheet();

}  // namespace pdfforge::ui
