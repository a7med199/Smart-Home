#pragma once
#include "SmartHome.h"
#include <QWidget>
#include <QLabel>
#include <QScrollArea>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QProgressBar>
#include <QEvent>

// ─── HourlyChart ─────────────────────────────────────────────────────────────
class HourlyChart : public QWidget {
    Q_OBJECT
public:
    explicit HourlyChart(QWidget* parent = nullptr);
    void setData(const QVector<double>& hourly, int currentHour);

protected:
    void paintEvent(QPaintEvent* ev) override;

private:
    QVector<double> m_data;
    int             m_hour = -1;
};

// ─── RoomsPage ────────────────────────────────────────────────────────────────
class RoomsPage : public QWidget {
    Q_OBJECT
public:
    explicit RoomsPage(SmartHome* home, QWidget* parent = nullptr);
    void refresh();
    // Refresh only if the user is not pressing a mouse button / dragging a slider
    void refreshIfIdle();
    void updateEnvironment();   // update temp/humidity/load texts in place (no rebuild)

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

private slots:
    void onRoomClicked(const QString& roomId);
    void onDeviceClicked(const QString& deviceId);
    void onSliderChanged(const QString& deviceId, int value);

private:
    void buildRoomCards();
    void buildDeviceCards(const QString& roomId = QString());
    QWidget* makeRoomCard(QSharedPointer<Room> r);
    QWidget* makeDeviceCard(QSharedPointer<Device> d);

    SmartHome* m_home = nullptr;
    QString    m_selectedRoom;

    QWidget*     m_roomsContainer   = nullptr;
    QWidget*     m_devicesContainer = nullptr;
    QLabel*      m_roomFilterLabel  = nullptr;
    QScrollArea* m_devScroll        = nullptr;   // devices list (scroll position is preserved)
    QGridLayout* m_roomsGrid        = nullptr;
    QGridLayout* m_devsGrid         = nullptr;
};

// ─── ClimatePage ─────────────────────────────────────────────────────────────
class ClimatePage : public QWidget {
    Q_OBJECT
public:
    explicit ClimatePage(SmartHome* home, QWidget* parent = nullptr);
    void refresh();
    void refreshIfIdle();
    void updateOutdoor();   // update the simulated outdoor-conditions cards
    void updateEnvironment();   // update temp/humidity/target texts in place (no rebuild)

private:
    void buildCards();
    QFrame*      m_outFrames[4] = {nullptr, nullptr, nullptr, nullptr};
    QLabel*      m_outVals[4]   = {nullptr, nullptr, nullptr, nullptr};
    SmartHome*   m_home      = nullptr;
    QWidget*     m_container = nullptr;
    QGridLayout* m_grid      = nullptr;
};

// ─── EnergyPage ──────────────────────────────────────────────────────────────
class EnergyPage : public QWidget {
    Q_OBJECT
public:
    explicit EnergyPage(SmartHome* home, QWidget* parent = nullptr);
    void refresh();

private:
    SmartHome*   m_home        = nullptr;
    QLabel*      m_lblLoad     = nullptr;
    QLabel*      m_lblDay      = nullptr;
    QLabel*      m_lblSolar    = nullptr;
    QLabel*      m_lblCost     = nullptr;
    QWidget*     m_barWidget   = nullptr;
    HourlyChart* m_chart       = nullptr;
    QGridLayout* m_barsLay     = nullptr;
    QVector<QProgressBar*> m_bars;      // reused between refreshes (no flicker)
    QVector<QLabel*>       m_barVals;
};

// ─── SecurityPage ─────────────────────────────────────────────────────────────
class SecurityPage : public QWidget {
    Q_OBJECT
public:
    explicit SecurityPage(SmartHome* home, QWidget* parent = nullptr);
    void refresh();

private:
    SmartHome* m_home            = nullptr;
    QWidget*   m_statusContainer = nullptr;
    QWidget*   m_alertContainer  = nullptr;
};

// ─── ScenesPage ──────────────────────────────────────────────────────────────
class ScenesPage : public QWidget {
    Q_OBJECT
public:
    explicit ScenesPage(SmartHome* home, QWidget* parent = nullptr);
    void refresh();

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    SmartHome*   m_home = nullptr;
    QGridLayout* m_grid = nullptr;
};

// ─── AutomationPage ──────────────────────────────────────────────────────────
class AutomationPage : public QWidget {
    Q_OBJECT
public:
    explicit AutomationPage(SmartHome* home, QWidget* parent = nullptr);
    void refresh();

private:
    SmartHome*   m_home    = nullptr;
    QVBoxLayout* m_listLay = nullptr;
};

// ─── LogPage ─────────────────────────────────────────────────────────────────
class LogPage : public QWidget {
    Q_OBJECT
public:
    explicit LogPage(SmartHome* home, QWidget* parent = nullptr);
    void refresh();

private:
    SmartHome*   m_home    = nullptr;
    QVBoxLayout* m_listLay = nullptr;
};
