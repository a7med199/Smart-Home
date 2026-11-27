#include "MainWindow.h"
#include <QApplication>
#include <QFont>
#include <QIcon>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    app.setApplicationName("Nexus Home");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("NexusTech");

    // Default application font
    QFont font("Segoe UI", 10);
    font.setStyleHint(QFont::SansSerif);
    app.setFont(font);

    MainWindow window;
    window.show();

    return app.exec();
}
