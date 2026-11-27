#pragma once
#include "Room.h"
#include "Models.h"
#include <QObject>
#include <QVector>
#include <QSharedPointer>
#include <QTimer>
#include <QDate>
#include <QDateTime>
#include <QMap>
#include <QStringList>
#include <QJsonDocument>

class SmartHome : public QObject {
    Q_OBJECT

public:
    explicit SmartHome(QObject* parent = nullptr);
    ~SmartHome() override = default;

    // ── Rooms ────────────────────────────────────────────────────────────────
    const QVector<QSharedPointer<Room>>& rooms()  const { return m_rooms; }
    QSharedPointer<Room> findRoom(const QString& id)   const;
    // Sets the room target AND keeps the room's thermostat device in sync
    void setRoomTargetTemp(const QString& roomId, int temp);

    // ── Devices (cross-room) ─────────────────────────────────────────────────
    QVector<QSharedPointer<Device>> allDevices()  const;
    QSharedPointer<Device> findDevice(const QString& id) const;
    void toggleDevice(const QString& deviceId);
    void setDeviceValue(const QString& deviceId, int value);

    // ── Aggregated stats ─────────────────────────────────────────────────────
    int    totalDeviceCount()  const;
    int    activeDeviceCount() const;
    double totalWatts()        const;
    double totalKwh()          const { return m_dailyKwh; }
    double solarWatts()        const { return m_solarWatts; }
    double solarKwh()          const { return m_solarKwh; }
    double estimatedCost()     const;   // net of solar generation

    // ── Security ─────────────────────────────────────────────────────────────
    bool alarmArmed()          const { return m_alarmArmed; }
    bool alarmTriggered()      const { return m_alarmTriggered; }
    void setAlarmArmed(bool a);
    bool homePresent()         const { return m_homePresent; }
    void setHomePresent(bool present);   // simulated phone presence (Departure Mode)
    const QVector<LogEntry>& alerts() const { return m_alerts; }
    void raiseAlert(const QString& room, const QString& msg,
                    LogEntry::Level level = LogEntry::Level::Warning,
                    const QString& deviceId = QString());
    void clearAlerts();

    // ── Simulated outdoor conditions (time-of-day model, not a real feed) ──
    double outdoorTemp()     const { return m_outTemp; }
    int    outdoorHumidity() const { return m_outHum; }
    int    airQuality()      const { return m_aqi; }
    double uvIndex()         const { return m_uv; }

    // ── Scenes ───────────────────────────────────────────────────────────────
    const QVector<Scene>& scenes()       const { return m_scenes; }
    void  activateScene(const QString& sceneId);

    // ── Automations ──────────────────────────────────────────────────────────
    const QVector<Automation>& automations() const { return m_automations; }
    void toggleAutomation(const QString& autoId);

    // ── Log ──────────────────────────────────────────────────────────────────
    const QVector<LogEntry>& log() const { return m_log; }
    void addLog(const QString& room, const QString& msg,
                LogEntry::Level level = LogEntry::Level::Info);

    // ── Hourly energy history ────────────────────────────────────────────────
    const QVector<double>& hourlyUsage() const { return m_hourlyUsage; }

    // ── Persistence ──────────────────────────────────────────────────────────
    bool saveToFile(const QString& path) const;
    bool loadFromFile(const QString& path);

signals:
    void devicesChanged();                 // structural change: pages must rebuild
    void statsChanged();                   // lightweight: numbers only (tick, slider)
    void roomEnvironmentChanged(const QString& roomId);
    void logUpdated();
    void alertsChanged();
    void alarmStateChanged(bool armed);
    void sirenTriggered(const QString& message);
    void sceneActivated(const QString& sceneId);

private slots:
    void onSimulationTick();

private:
    void buildDefaultHome();
    void buildScenes();
    void buildAutomations();
    void syncThermostats();     // room target <- thermostat device value
    void clearActiveScene();
    void updateThermostatLoads();
    void updateOutdoor(const QDateTime& now);
    bool runAutomations(const QDateTime& now);   // returns true if devices changed
    bool autoEnabled(const QString& id) const;
    void autoLog(const QString& msg);
    void motionLights(const QString& roomId);

    QVector<QSharedPointer<Room>> m_rooms;
    QVector<Scene>                m_scenes;
    QVector<Automation>           m_automations;
    QVector<LogEntry>             m_log;
    QVector<LogEntry>             m_alerts;
    QVector<double>               m_hourlyUsage;

    bool   m_alarmArmed  = false;
    double m_dailyKwh    = 0.0;
    double m_solarWatts  = 0.0;
    double m_solarKwh    = 0.0;
    QDate  m_lastDate;

    bool   m_alarmTriggered = false;
    bool   m_homePresent    = true;

    double m_outTemp  = 28.0;
    double m_outNoise = 0.0;
    int    m_outHum   = 60;
    int    m_aqi      = 45;
    double m_uv       = 0.0;

    int                 m_climateBand = -99;   // Climate Guard: last applied band
    QMap<QString,QDate> m_lastRun;             // once-per-day automation guards
    QStringList         m_energySaverOff;      // plugs switched off by Energy Saver

    QTimer* m_simTimer   = nullptr;
};
