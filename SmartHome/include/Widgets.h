#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QButtonGroup>

// ─── HeaderBar ────────────────────────────────────────────────────────────────
class HeaderBar : public QWidget {
    Q_OBJECT
public:
    explicit HeaderBar(QWidget* parent = nullptr);

    void setDeviceCount(int total, int active);
    void setPower(double kw);
    void setTime(const QString& t);

private:
    QLabel* m_lblDevices = nullptr;
    QLabel* m_lblActive  = nullptr;
    QLabel* m_lblPower   = nullptr;
    QLabel* m_lblClock   = nullptr;
};

// ─── SideBar ──────────────────────────────────────────────────────────────────
class SideBar : public QWidget {
    Q_OBJECT
public:
    explicit SideBar(QWidget* parent = nullptr);

signals:
    void pageRequested(int index);

private:
    void addNavButton(const QString& icon, const QString& label, int index);
    QButtonGroup* m_group = nullptr;
    int           m_count = 0;
};
