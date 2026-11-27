#include "MainWindow.h"
#include "Widgets.h"
#include "Pages.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDateTime>
#include <QStatusBar>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Nexus Home — Smart Home Simulator");
    setMinimumSize(1100, 720);
    resize(1280, 800);

    applyStyleSheet();
    m_home = new SmartHome(this);
    setupUI();
    setupConnections();

    m_clockTimer = new QTimer(this);
    connect(m_clockTimer, &QTimer::timeout, this, &MainWindow::updateClock);
    m_clockTimer->start(1000);

    onDevicesChanged();
    updateClock();

    // ── File menu ────────────────────────────────────────────────────────────
    auto* file = menuBar()->addMenu("&File");
    connect(file->addAction("&Save State"), &QAction::triggered, this, [this]{
        QString p = QFileDialog::getSaveFileName(this, "Save", "", "JSON (*.json)");
        if (!p.isEmpty()) {
            if (m_home->saveToFile(p))
                statusBar()->showMessage("  ● State saved successfully", 3000);
            else
                QMessageBox::warning(this, "Save Failed", "Could not write to file.");
        }
    });
    connect(file->addAction("&Load State"), &QAction::triggered, this, [this]{
        QString p = QFileDialog::getOpenFileName(this, "Load", "", "JSON (*.json)");
        if (!p.isEmpty()) {
            if (m_home->loadFromFile(p)) {
                onDevicesChanged();
                statusBar()->showMessage("  ● State loaded successfully", 3000);
            } else {
                QMessageBox::warning(this, "Load Failed",
                                     "Could not read file (missing or not a valid state file).");
            }
        }
    });
    file->addSeparator();
    connect(file->addAction("&Quit"), &QAction::triggered, qApp, &QApplication::quit);

    // ── Simulate menu ────────────────────────────────────────────────────────
    // FIX: alerts go through the model, so they show up on the Security page,
    // flag the related sensor device, and can be cleared there.
    auto* sim = menuBar()->addMenu("&Simulate");
    connect(sim->addAction("Motion Alert"), &QAction::triggered, this, [this]{
        m_home->raiseAlert("Garden", "Motion detected — Garden camera",
                           LogEntry::Level::Warning, "g4");
    });
    connect(sim->addAction("Smoke Alarm"), &QAction::triggered, this, [this]{
        m_home->raiseAlert("Kitchen", "Smoke sensor triggered!",
                           LogEntry::Level::Danger, "k4");
    });
    sim->addSeparator();
    connect(sim->addAction("Phone Leaves Home"), &QAction::triggered, this, [this]{
        m_home->setHomePresent(false);
    });
    connect(sim->addAction("Phone Arrives Home"), &QAction::triggered, this, [this]{
        m_home->setHomePresent(true);
    });
}

void MainWindow::setupUI() {
    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* rootLay = new QVBoxLayout(central);
    rootLay->setContentsMargins(0,0,0,0);
    rootLay->setSpacing(0);

    m_header = new HeaderBar(this);
    rootLay->addWidget(m_header);

    auto* body = new QHBoxLayout;
    body->setContentsMargins(0,0,0,0);
    body->setSpacing(0);

    m_sidebar = new SideBar(this);
    body->addWidget(m_sidebar);

    auto* awayBtn  = m_sidebar->findChild<QPushButton*>("BtnAway");
    auto* nightBtn = m_sidebar->findChild<QPushButton*>("BtnNight");
    auto* alarmBtn = m_sidebar->findChild<QPushButton*>("BtnAlarm");

    if (awayBtn)  connect(awayBtn,  &QPushButton::clicked, this, [this]{ m_home->activateScene("away");  });
    if (nightBtn) connect(nightBtn, &QPushButton::clicked, this, [this]{ m_home->activateScene("night"); });
    if (alarmBtn) connect(alarmBtn, &QPushButton::clicked, this, [this]{
        // FIX: no manual security refresh here; alarmStateChanged handles it (once)
        m_home->setAlarmArmed(!m_home->alarmArmed());
    });

    m_stack = new QStackedWidget(this);

    m_roomsPage    = new RoomsPage(m_home, m_stack);
    m_climatePage  = new ClimatePage(m_home, m_stack);
    m_energyPage   = new EnergyPage(m_home, m_stack);
    m_securityPage = new SecurityPage(m_home, m_stack);
    m_scenesPage   = new ScenesPage(m_home, m_stack);
    m_autoPage     = new AutomationPage(m_home, m_stack);
    m_logPage      = new LogPage(m_home, m_stack);

    m_stack->addWidget(m_roomsPage);
    m_stack->addWidget(m_climatePage);
    m_stack->addWidget(m_energyPage);
    m_stack->addWidget(m_securityPage);
    m_stack->addWidget(m_scenesPage);
    m_stack->addWidget(m_autoPage);
    m_stack->addWidget(m_logPage);

    body->addWidget(m_stack, 1);
    rootLay->addLayout(body, 1);

    statusBar()->showMessage("  ● System Online  |  Nexus Home v1.0");
    statusBar()->setObjectName("StatusBar");
}

void MainWindow::setupConnections() {
    connect(m_sidebar, &SideBar::pageRequested, this, &MainWindow::navigateTo);

    connect(m_home, &SmartHome::devicesChanged,
            this, &MainWindow::onDevicesChanged);
    connect(m_home, &SmartHome::statsChanged,
            this, &MainWindow::onStatsChanged);
    connect(m_home, &SmartHome::alertsChanged,
            this, &MainWindow::onAlertsChanged);
    connect(m_home, &SmartHome::logUpdated,
            this, &MainWindow::onLogUpdated);
    connect(m_home, &SmartHome::roomEnvironmentChanged,
            this, &MainWindow::onRoomEnvironmentChanged);

    connect(m_home, &SmartHome::sirenTriggered, this, [this](const QString& msg){
        statusBar()->showMessage("  🚨 " + msg, 8000);
    });
    connect(m_home, &SmartHome::alarmStateChanged, this, [this](bool armed){
        statusBar()->showMessage(
            armed ? "  ⚠ ALARM ARMED" : "  ● Alarm Disarmed", 4000);
        if (m_stack->currentIndex() == 3) m_securityPage->refresh();
    });
}

void MainWindow::navigateTo(int index) {
    m_stack->setCurrentIndex(index);
    switch (index) {
        case 0: m_roomsPage   ->refresh(); break;
        case 1: m_climatePage ->refresh(); break;
        case 2: m_energyPage  ->refresh(); break;
        case 3: m_securityPage->refresh(); break;
        case 4: m_scenesPage  ->refresh(); break;
        case 5: m_autoPage    ->refresh(); break;
        case 6: m_logPage     ->refresh(); break;
    }
}

void MainWindow::updateHeader() {
    m_header->setDeviceCount(m_home->totalDeviceCount(), m_home->activeDeviceCount());
    m_header->setPower(m_home->totalWatts() / 1000.0);
}

// Structural change (toggle, scene, load...): rebuild the visible page once.
void MainWindow::onDevicesChanged() {
    updateHeader();
    navigateTo(m_stack->currentIndex());
}

// Simulation tick / slider drag: update numbers only, never rebuild cards.
void MainWindow::onStatsChanged() {
    updateHeader();
    if (m_stack->currentIndex() == 2) m_energyPage->refresh();
    if (m_stack->currentIndex() == 1) m_climatePage->updateOutdoor();
    if (m_stack->currentIndex() == 0) m_roomsPage->updateEnvironment();   // keeps LOAD live
}

void MainWindow::onAlertsChanged() {
    onDevicesChanged();   // device alert flags changed too; refresh current page once
}

void MainWindow::onLogUpdated() {
    if (m_stack->currentIndex() == 6) m_logPage->refresh();
}

// FIX: several rooms can change in one tick; coalesce them into ONE refresh,
// and skip it while the user is pressing/dragging something.
void MainWindow::onRoomEnvironmentChanged(const QString&) {
    if (m_envRefreshPending) return;
    m_envRefreshPending = true;
    QTimer::singleShot(0, this, [this]{
        m_envRefreshPending = false;
        // in-place text updates: no widgets are destroyed, so no flicker
        if (m_stack->currentIndex() == 0) m_roomsPage->updateEnvironment();
        if (m_stack->currentIndex() == 1) m_climatePage->updateEnvironment();
    });
}

void MainWindow::updateClock() {
    m_header->setTime(QDateTime::currentDateTime().toString("hh:mm:ss"));
}

void MainWindow::applyStyleSheet() {
    setStyleSheet(R"(
QMainWindow, QWidget {
    background-color: #070b14;
    color: #e0f0ff;
    font-family: "Segoe UI", "Ubuntu", sans-serif;
    font-size: 13px;
}
QMenuBar {
    background: #0c1220;
    border-bottom: 1px solid rgba(0,180,255,0.15);
    color: #7a9bb5;
    padding: 2px 8px;
}
QMenuBar::item:selected { background: rgba(0,180,255,0.12); color: #e0f0ff; }
QMenu { background: #0c1220; border: 1px solid rgba(0,180,255,0.2); }
QMenu::item:selected { background: rgba(0,180,255,0.15); }

#HeaderBar {
    background: #0c1220;
    border-bottom: 1px solid rgba(0,180,255,0.15);
}
#LogoLabel {
    font-size: 17px; font-weight: 700; letter-spacing: 2px; color: #00b4ff;
}
#StatValue {
    font-size: 18px; font-weight: 700; color: #00b4ff;
    font-family: "Courier New", monospace;
}
#StatLabel { font-size: 10px; color: #7a9bb5; letter-spacing: 1px; }
#ClockLabel {
    font-size: 20px; color: #00ffc2; letter-spacing: 3px;
    font-family: "Courier New", monospace;
}

#SideBar {
    background: #0c1220;
    border-right: 1px solid rgba(0,180,255,0.15);
}
#NavBtn {
    text-align: left; padding: 4px 10px;
    color: #7a9bb5; border: 1px solid transparent;
    border-radius: 6px; font-size: 12px; font-weight: 600;
    letter-spacing: 1px;
}
#NavBtn:hover   { color: #e0f0ff; background: rgba(0,180,255,0.07); }
#NavBtn:checked { color: #00b4ff; background: rgba(0,180,255,0.13);
                  border-color: rgba(0,180,255,0.25); }

#BtnAway, #BtnNight {
    background: transparent; border: 1px solid rgba(0,180,255,0.2);
    color: #7a9bb5; border-radius: 5px; font-size: 11px; padding: 4px;
}
#BtnAway:hover, #BtnNight:hover { color: #00b4ff; border-color: #00b4ff; }
#BtnAlarm {
    background: transparent; border: 1px solid rgba(255,71,87,0.3);
    color: #ff4757; border-radius: 5px; font-size: 11px; padding: 4px;
}
#BtnAlarm:hover { background: rgba(255,71,87,0.12); }

#SectionTitle {
    font-size: 11px; letter-spacing: 3px; color: #7a9bb5;
    text-transform: uppercase; padding: 0 0 6px 0;
}
#HLine { color: rgba(0,180,255,0.12); }

#RoomCard, #RoomCardSelected {
    background: #0c1220; border: 1px solid rgba(0,180,255,0.15);
    border-radius: 10px;
}
#RoomCardSelected { border-color: rgba(0,180,255,0.4); background: #111827; }
#RoomCard:hover   { border-color: rgba(0,180,255,0.35); }
#RoomName   { font-size: 13px; font-weight: 600; letter-spacing: 1px; }
#RoomEmoji  { font-size: 22px; }
#DotOn  { color: #2ed573; font-size: 9px; }
#DotOff { color: #3a4a5a; font-size: 9px; }

#DeviceCard, #DeviceCardOn {
    background: #070b14; border: 1px solid rgba(0,180,255,0.15);
    border-radius: 8px;
}
#DeviceCardOn { border-color: rgba(255,159,67,0.3); background: rgba(255,159,67,0.05); }
#DeviceIcon   { font-size: 24px; }
#DeviceName   { font-size: 11px; font-weight: 600; color: #e0f0ff; }
#DeviceStatus { font-size: 10px; color: #7a9bb5;
                font-family: "Courier New", monospace; }
#DeviceCardOn #DeviceStatus { color: #ff9f43; }
#BadgeOn  { color: #2ed573; font-size: 9px; }
#BadgeOff { color: #3a4a5a; font-size: 9px; }

#MiniSlider { margin: 3px 0; }
QSlider::groove:horizontal {
    height: 4px; background: rgba(255,255,255,0.08); border-radius: 2px;
}
QSlider::handle:horizontal {
    width: 12px; height: 12px; margin: -4px 0;
    background: #00b4ff; border-radius: 6px;
}
QSlider::sub-page:horizontal { background: rgba(0,180,255,0.5); border-radius: 2px; }

#ClimateCard {
    background: #0c1220; border: 1px solid rgba(0,180,255,0.15); border-radius:10px;
}
#ThermoDisplay { font-size: 36px; font-weight:700; color:#00b4ff;
                 font-family:"Courier New",monospace; }
#ThermoTarget  { font-size:12px; color:#7a9bb5; font-family:"Courier New",monospace; }
#ThermoBtn {
    background: #070b14; border: 1px solid rgba(0,180,255,0.2);
    color: #e0f0ff; border-radius:5px; padding:4px 12px; font-size:16px;
}
#ThermoBtn:hover { background: rgba(0,180,255,0.15); border-color:#00b4ff; }

#StatCard, #StatCardGreen, #StatCardWarn {
    background: #0c1220; border: 1px solid rgba(0,180,255,0.15); border-radius:10px;
}
#StatCard #StatValue    { font-size:22px; font-weight:700; color:#00b4ff;
                           font-family:"Courier New",monospace; }
#StatCardGreen #StatValue { color:#2ed573; }
#StatCardWarn  #StatValue { color:#ff9f43; }

#ChartWidget { background:#0c1220; border:1px solid rgba(0,180,255,0.15); border-radius:8px; }

#BarLabel { font-size:11px; color:#7a9bb5; min-width:90px; }
#BarVal   { font-size:11px; color:#e0f0ff; font-family:"Courier New",monospace; min-width:50px; }
QProgressBar { border-radius:3px; background:rgba(255,255,255,0.06); }
#BarDefault::chunk { background:#00b4ff; border-radius:3px; }
#BarWarm::chunk    { background:#ff9f43; border-radius:3px; }
#BarGreen::chunk   { background:#2ed573; border-radius:3px; }

#SecCardSafe, #SecCardArmed {
    background:#0c1220; border:1px solid rgba(46,213,115,0.25); border-radius:10px;
    min-width:130px; min-height:110px;
}
#SecCardArmed { border-color:rgba(255,71,87,0.35); background:rgba(255,71,87,0.05); }
#SecIcon  { font-size:28px; }
#SecValSafe  { font-size:14px; font-weight:600; color:#2ed573; }
#SecValArmed { font-size:14px; font-weight:600; color:#ff4757; }

#AlertInfo { border-left:3px solid #00b4ff; border-radius:0 6px 6px 0;
             background:#0c1220; }
#AlertWarn { border-left:3px solid #ff9f43; border-radius:0 6px 6px 0;
             background:#0c1220; }
#AlertDanger { border-left:3px solid #ff4757; border-radius:0 6px 6px 0;
               background:rgba(255,71,87,0.08); }
#AlertDot { font-size:12px; color:#7a9bb5; }
#AlertMsg { font-size:12px; color:#e0f0ff; }
#ClearBtn {
    background: transparent; border: 1px solid rgba(0,180,255,0.2);
    color: #7a9bb5; border-radius: 4px; font-size: 10px; padding: 2px 10px;
}
#ClearBtn:hover { color: #00b4ff; border-color: #00b4ff; }

#SceneCard, #SceneCardActive {
    background:#0c1220; border:1px solid rgba(0,180,255,0.15); border-radius:10px;
}
#SceneCardActive { border-color:rgba(0,255,194,0.4); background:rgba(0,255,194,0.05); }
#SceneIcon { font-size:28px; }
#SceneName { font-size:12px; font-weight:600; letter-spacing:1px; color:#e0f0ff; }
#SceneDesc { font-size:10px; color:#7a9bb5; }

#AutoRow, #AutoRowOn {
    background:#0c1220; border:1px solid rgba(0,180,255,0.15); border-radius:8px;
}
#AutoRowOn { border-color:rgba(0,255,194,0.25); }
#AutoIcon { font-size:20px; }
#AutoName { font-size:13px; font-weight:600; color:#e0f0ff; }
#AutoDesc { font-size:11px; color:#7a9bb5; }
#ToggleOn  { background:rgba(0,255,194,0.2); border:1px solid rgba(0,255,194,0.4);
             color:#00ffc2; border-radius:4px; font-size:10px; font-weight:700; }
#ToggleOff { background:rgba(255,255,255,0.06); border:1px solid rgba(255,255,255,0.1);
             color:#7a9bb5; border-radius:4px; font-size:10px; }

#LogTime { font-size:11px; color:rgba(0,180,255,0.5); min-width:40px;
           font-family:"Courier New",monospace; }
#LogRoom { font-size:11px; color:#a29bfe; min-width:90px; }
#LogMsg  { font-size:11px; color:#e0f0ff; }
#LogMsgWarn    { font-size:11px; color:#ff9f43; }
#LogMsgDanger  { font-size:11px; color:#ff4757; font-weight:600; }
#LogMsgSuccess { font-size:11px; color:#2ed573; }

QScrollArea, #ScrollArea { background:transparent; border:none; }
QScrollBar:vertical { background:#070b14; width:6px; }
QScrollBar::handle:vertical { background:rgba(0,180,255,0.2); border-radius:3px; }

QStatusBar { background:#0c1220; color:#7a9bb5;
             border-top:1px solid rgba(0,180,255,0.12); font-size:11px; }
)");
}
