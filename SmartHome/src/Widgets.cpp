#include "Widgets.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QAbstractButton>

// ════════════════════════════════════════════════════════════════════════════
// HeaderBar
// ════════════════════════════════════════════════════════════════════════════

HeaderBar::HeaderBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(56);
    setObjectName("HeaderBar");

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(20, 0, 20, 0);
    lay->setSpacing(0);

    // Logo
    auto* logo = new QLabel("⌂  NEXUS HOME", this);
    logo->setObjectName("LogoLabel");
    lay->addWidget(logo);
    lay->addStretch();

    // Stats
    auto makeStatBox = [&](const QString& lbl, QLabel*& val) {
        auto* box  = new QWidget(this);
        auto* blay = new QVBoxLayout(box);
        blay->setContentsMargins(0,0,0,0);
        blay->setSpacing(2);
        val = new QLabel("—", box);
        val->setObjectName("StatValue");
        val->setAlignment(Qt::AlignCenter);
        auto* cap = new QLabel(lbl, box);
        cap->setObjectName("StatLabel");
        cap->setAlignment(Qt::AlignCenter);
        blay->addWidget(val);
        blay->addWidget(cap);
        lay->addWidget(box);
        lay->addSpacing(24);
    };

    makeStatBox("DEVICES", m_lblDevices);
    makeStatBox("ACTIVE",  m_lblActive);
    makeStatBox("POWER",   m_lblPower);
    lay->addSpacing(12);

    m_lblClock = new QLabel("00:00:00", this);
    m_lblClock->setObjectName("ClockLabel");
    lay->addWidget(m_lblClock);
}

void HeaderBar::setDeviceCount(int total, int active) {
    m_lblDevices->setText(QString::number(total));
    m_lblActive ->setText(QString::number(active));
}

void HeaderBar::setPower(double kw) {
    m_lblPower->setText(QString("%1 kW").arg(kw, 0, 'f', 1));
}

void HeaderBar::setTime(const QString& t) {
    m_lblClock->setText(t);
}

// ════════════════════════════════════════════════════════════════════════════
// SideBar
// ════════════════════════════════════════════════════════════════════════════

SideBar::SideBar(QWidget* parent) : QWidget(parent) {
    setFixedWidth(150);
    setObjectName("SideBar");

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(8, 12, 8, 12);
    lay->setSpacing(4);

    m_group = new QButtonGroup(this);
    m_group->setExclusive(true);

    addNavButton("⊞", "ROOMS",    0);
    addNavButton("◈", "CLIMATE",  1);
    addNavButton("⚡", "ENERGY",   2);
    addNavButton("◉", "SECURITY", 3);
    addNavButton("✦", "SCENES",   4);
    addNavButton("⚙", "AUTO",     5);
    addNavButton("≡", "LOG",      6);

    lay->addStretch();

    // Bottom quick-action buttons
    auto makeBottom = [&](const QString& label, const QString& objName) {
        auto* btn = new QPushButton(label, this);
        btn->setObjectName(objName);
        btn->setFixedHeight(30);
        lay->addWidget(btn);
        lay->addSpacing(3);
        return btn;
    };
    makeBottom("AWAY MODE",  "BtnAway");
    makeBottom("NIGHT MODE", "BtnNight");
    makeBottom("⚠ ALARM",   "BtnAlarm");

    // Activate first nav button
    auto* first = m_group->button(0);
    if (first) {
        first->setChecked(true);
        first->setProperty("active", true);
    }
}

void SideBar::addNavButton(const QString& icon, const QString& label, int index) {
    auto* btn = new QPushButton(QString("  %1  %2").arg(icon).arg(label), this);
    btn->setObjectName("NavBtn");
    btn->setCheckable(true);
    btn->setFixedHeight(38);
    btn->setFlat(true);
    m_group->addButton(btn, index);

    connect(btn, &QPushButton::clicked, this, [this, index]() {
        emit pageRequested(index);
    });

    // FIX: the stretch is added AFTER the nav buttons, so insertWidget(count()-1)
    // put every new button before the previous one (ROOMS ended up last).
    // Plain addWidget keeps the intended order.
    layout()->addWidget(btn);
    ++m_count;
}
