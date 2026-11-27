#pragma once
#include "SmartHome.h"
#include <QMainWindow>
#include <QStackedWidget>
#include <QLabel>
#include <QTimer>

class QListWidget;
class QScrollArea;
class RoomsPage;
class ClimatePage;
class EnergyPage;
class SecurityPage;
class ScenesPage;
class AutomationPage;
class LogPage;
class HeaderBar;
class SideBar;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void navigateTo(int index);
    void onDevicesChanged();     // structural change -> rebuild current page
    void onStatsChanged();       // tick / slider -> header + energy page only
    void onAlertsChanged();
    void onLogUpdated();
    void onRoomEnvironmentChanged(const QString& roomId);
    void updateClock();

private:
    void setupUI();
    void setupConnections();
    void applyStyleSheet();
    void updateHeader();

    SmartHome*     m_home   = nullptr;
    HeaderBar*     m_header = nullptr;
    SideBar*       m_sidebar= nullptr;
    QStackedWidget*m_stack  = nullptr;

    RoomsPage*     m_roomsPage     = nullptr;
    ClimatePage*   m_climatePage   = nullptr;
    EnergyPage*    m_energyPage    = nullptr;
    SecurityPage*  m_securityPage  = nullptr;
    ScenesPage*    m_scenesPage    = nullptr;
    AutomationPage*m_autoPage      = nullptr;
    LogPage*       m_logPage       = nullptr;

    QTimer*        m_clockTimer    = nullptr;
    bool           m_envRefreshPending = false;
};
