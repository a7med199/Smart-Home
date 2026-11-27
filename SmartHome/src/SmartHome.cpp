#include "SmartHome.h"
#include <QDateTime>
#include <QRandomGenerator>
#include <QJsonArray>
#include <QJsonParseError>
#include <QFile>
#include <QMap>
#include <QtMath>
#include <algorithm>

namespace {
const double kPi = 3.14159265358979323846;

// Simple bell-curve solar model: 0 W before 06:00 and after 18:00, 1800 W peak at noon
double solarAt(const QTime& t) {
    double h = t.hour() + t.minute() / 60.0;
    if (h <= 6.0 || h >= 18.0) return 0.0;
    return 1800.0 * qSin(kPi * (h - 6.0) / 12.0);
}

// Integral of solarAt() from 00:00 to the given time, in kWh
double solarKwhUntil(const QTime& t) {
    double h = t.hour() + t.minute() / 60.0;
    if (h <= 6.0) return 0.0;
    double hh = qMin(h, 18.0);
    return 1.8 * (12.0 / kPi) * (1.0 - qCos(kPi * (hh - 6.0) / 12.0));
}
} // namespace

// ─── Construction ──────────────────────────────────────────────────────────

SmartHome::SmartHome(QObject* parent) : QObject(parent) {
    const QDateTime now = QDateTime::currentDateTime();
    const int nowH = now.time().hour();

    // m_hourlyUsage holds ENERGY in kWh per hour. Past hours get simulated
    // history; the current hour starts empty and accumulates on each tick.
    // m_dailyKwh is always the sum of m_hourlyUsage.
    m_hourlyUsage.fill(0.0, 24);
    m_dailyKwh = 0.0;
    for (int i = 0; i < nowH; ++i) {
        m_hourlyUsage[i] = 1.0 + QRandomGenerator::global()->generateDouble() * 4.0;
        m_dailyKwh += m_hourlyUsage[i];
    }
    m_lastDate   = now.date();
    m_solarWatts = solarAt(now.time());
    m_solarKwh   = solarKwhUntil(now.time());   // midnight -> now; ticks add now -> future
    updateOutdoor(now);

    buildDefaultHome();
    buildScenes();
    buildAutomations();

    addLog("SYSTEM", "Nexus Home initialised");
    addLog("SYSTEM", "All subsystems online");

    m_simTimer = new QTimer(this);
    connect(m_simTimer, &QTimer::timeout, this, &SmartHome::onSimulationTick);
    m_simTimer->start(3000);
}

// ─── Room / Device access ──────────────────────────────────────────────────

QSharedPointer<Room> SmartHome::findRoom(const QString& id) const {
    for (auto& r : m_rooms) if (r->id() == id) return r;
    return nullptr;
}

// Single source of truth: room target <-> thermostat device value
void SmartHome::setRoomTargetTemp(const QString& roomId, int temp) {
    auto room = findRoom(roomId);
    if (!room) return;
    room->setTargetTemp(temp);
    for (auto& d : room->devices())
        if (d->type() == DeviceType::Thermostat) d->setValue(room->targetTemp());
    clearActiveScene();
    updateThermostatLoads();
    emit statsChanged();
}

void SmartHome::syncThermostats() {
    for (auto& r : m_rooms)
        for (auto& d : r->devices())
            if (d->type() == DeviceType::Thermostat) r->setTargetTemp(d->value());
}

// Thermostat power follows how far the room is from its target:
// ~5% standby when at target, up to 100% when 2 degrees or more away.
void SmartHome::updateThermostatLoads() {
    for (auto& r : m_rooms) {
        const double diff = qAbs(r->temperature() - r->targetTemp());
        const double load = diff < 0.3 ? 0.05 : qBound(0.2, diff / 2.0, 1.0);
        for (auto& d : r->devices())
            if (d->type() == DeviceType::Thermostat) d->setLoadFactor(load);
    }
}

void SmartHome::clearActiveScene() {
    for (auto& s : m_scenes) s.active = false;
}

QVector<QSharedPointer<Device>> SmartHome::allDevices() const {
    QVector<QSharedPointer<Device>> all;
    for (auto& r : m_rooms) all << r->devices();
    return all;
}

QSharedPointer<Device> SmartHome::findDevice(const QString& id) const {
    for (auto& r : m_rooms)
        if (auto d = r->findDevice(id)) return d;
    return nullptr;
}

void SmartHome::toggleDevice(const QString& deviceId) {
    auto dev = findDevice(deviceId);
    if (!dev) return;
    dev->toggle();
    clearActiveScene();
    updateThermostatLoads();
    auto room = findRoom(dev->roomId());
    addLog(room ? room->name() : "?",
           QString("%1 turned %2").arg(dev->name()).arg(dev->isOn() ? "ON" : "OFF"));
    emit devicesChanged();
}

// Only emits statsChanged (cheap) so pages/sliders are NOT rebuilt mid-drag
void SmartHome::setDeviceValue(const QString& deviceId, int value) {
    auto dev = findDevice(deviceId);
    if (!dev) return;
    dev->setValue(value);
    if (dev->type() == DeviceType::Thermostat) {
        if (auto room = findRoom(dev->roomId())) room->setTargetTemp(dev->value());
        updateThermostatLoads();
    }
    clearActiveScene();
    emit statsChanged();
}

// ─── Aggregates ────────────────────────────────────────────────────────────

int SmartHome::totalDeviceCount() const {
    int n = 0;
    for (auto& r : m_rooms) n += r->devices().size();
    return n;
}

int SmartHome::activeDeviceCount() const {
    int n = 0;
    for (auto& r : m_rooms) n += r->activeDeviceCount();
    return n;
}

double SmartHome::totalWatts() const {
    double w = 0;
    for (auto& r : m_rooms) w += r->totalWatts();
    return w;
}

double SmartHome::estimatedCost() const {
    return qMax(0.0, m_dailyKwh - m_solarKwh) * 0.12;
}

// ─── Security ──────────────────────────────────────────────────────────────

void SmartHome::setAlarmArmed(bool a) {
    if (m_alarmArmed == a) return;
    m_alarmArmed = a;
    if (!a) m_alarmTriggered = false;   // disarming silences the siren
    addLog("SECURITY", a ? "Alarm ARMED" : "Alarm DISARMED",
           a ? LogEntry::Level::Warning : LogEntry::Level::Success);
    emit alarmStateChanged(a);
}

// Behaviour depends on the alarm state:
//  - Armed (or a smoke sensor, which always sounds): the siren triggers and the
//    event is recorded as a SECURITY INCIDENT.
//  - Disarmed: the event is only logged, no siren.
void SmartHome::raiseAlert(const QString& room, const QString& msg,
                           LogEntry::Level level, const QString& deviceId) {
    QSharedPointer<Device> dev;
    if (!deviceId.isEmpty()) dev = findDevice(deviceId);
    if (dev) dev->setAlert(true);

    const bool siren = m_alarmArmed || (dev && dev->type() == DeviceType::SmokeSensor);

    LogEntry e;
    e.timestamp = QDateTime::currentDateTime().toString("hh:mm");
    e.room      = room;
    e.deviceId  = deviceId;
    if (siren) {
        e.level   = LogEntry::Level::Danger;
        e.message = "SECURITY INCIDENT — " + msg;
        m_alarmTriggered = true;
    } else {
        e.level   = (level == LogEntry::Level::Danger) ? LogEntry::Level::Warning : level;
        e.message = msg + " (alarm disarmed — logged only)";
    }
    m_alerts.prepend(e);
    if (m_alerts.size() > 20) m_alerts.removeLast();

    addLog(room, e.message, e.level);

    if (dev && dev->type() == DeviceType::Sensor) motionLights(dev->roomId());

    if (siren) emit sirenTriggered(e.message);
    emit alertsChanged();
}

// Clears only the alerts in the list, and only the device flags tied to them
void SmartHome::clearAlerts() {
    for (auto& a : m_alerts) {
        if (a.deviceId.isEmpty()) continue;
        if (auto d = findDevice(a.deviceId)) d->setAlert(false);
    }
    m_alerts.clear();
    m_alarmTriggered = false;
    addLog("SECURITY", "Alerts cleared", LogEntry::Level::Success);
    emit alertsChanged();
}

// ─── Scenes ────────────────────────────────────────────────────────────────

void SmartHome::activateScene(const QString& sceneId) {
    // Validate first: an unknown id must not deactivate the current scene
    auto it = std::find_if(m_scenes.begin(), m_scenes.end(),
                           [&](const Scene& s){ return s.id == sceneId; });
    if (it == m_scenes.end()) return;

    for (auto& s : m_scenes) s.active = false;
    it->active = true;
    const QString sceneName = it->name;

    auto devs = allDevices();
    if (sceneId == "morning") {
        for (auto& d : devs) {
            if (d->type() == DeviceType::Light) {
                if (!d->isOn()) d->toggle();
                d->setValue(60);
            }
            if (d->type() == DeviceType::Thermostat) {
                if (!d->isOn()) d->toggle();
                d->setValue(22);
            }
        }
    } else if (sceneId == "cinema") {
        for (auto& d : devs) {
            if (d->type() == DeviceType::Light) {
                if (!d->isOn()) d->toggle();
                d->setValue(15);
            }
            if (d->type() == DeviceType::TV || d->type() == DeviceType::Speaker) {
                if (!d->isOn()) d->toggle();
            }
        }
    } else if (sceneId == "dinner") {
        for (auto& d : devs) {
            if (d->type() == DeviceType::Light) {
                if (!d->isOn()) d->toggle();
                d->setValue(40);
            }
        }
    } else if (sceneId == "away") {
        for (auto& d : devs) {
            const bool security = d->type() == DeviceType::Lock        ||
                                  d->type() == DeviceType::Camera      ||
                                  d->type() == DeviceType::Sensor      ||
                                  d->type() == DeviceType::SmokeSensor;
            if (!security) {
                if (d->isOn()) d->toggle();
            } else if (!d->isOn()) {
                d->toggle();
            }
        }
    } else if (sceneId == "night") {
        for (auto& d : devs) {
            if (d->type() == DeviceType::Light   && d->isOn()) d->toggle();
            if (d->type() == DeviceType::TV      && d->isOn()) d->toggle();
            if (d->type() == DeviceType::Thermostat) {
                if (!d->isOn()) d->toggle();
                d->setValue(18);
            }
        }
    } else if (sceneId == "party") {
        for (auto& d : devs) {
            if (d->type() == DeviceType::Light) {
                if (!d->isOn()) d->toggle();
                d->setValue(100);
            }
            if (d->type() == DeviceType::Speaker) {
                if (!d->isOn()) d->toggle();
            }
        }
    }

    syncThermostats();
    updateThermostatLoads();
    addLog("SYSTEM", QString("Scene activated: %1").arg(sceneName));

    // Away mode also arms the alarm system itself
    if (sceneId == "away") setAlarmArmed(true);

    emit sceneActivated(sceneId);
    emit devicesChanged();
}

// ─── Automations ──────────────────────────────────────────────────────────

bool SmartHome::autoEnabled(const QString& id) const {
    for (auto& a : m_automations) if (a.id == id) return a.enabled;
    return false;
}

void SmartHome::autoLog(const QString& msg) {
    addLog("AUTO", msg, LogEntry::Level::Success);
}

void SmartHome::toggleAutomation(const QString& autoId) {
    for (auto& a : m_automations) {
        if (a.id != autoId) continue;
        a.enabled = !a.enabled;
        addLog("AUTO", QString("%1 %2").arg(a.name)
               .arg(a.enabled ? "enabled" : "disabled"));

        // Let the rule fire again right away when (re)enabled
        m_lastRun.remove(autoId);
        m_lastRun.remove(autoId + "-end");
        if (autoId == "a3") m_climateBand = -99;
        break;
    }
}

// Departure Mode (a2): driven by the simulated phone presence
void SmartHome::setHomePresent(bool present) {
    if (m_homePresent == present) return;
    m_homePresent = present;
    addLog("SYSTEM", present ? "Phone arrived home" : "Phone left home");
    if (!autoEnabled("a2")) return;

    if (!present) {
        autoLog("Departure Mode: everything off, alarm armed");
        activateScene("away");
    } else {
        autoLog("Departure Mode: welcome home, alarm disarmed");
        setAlarmArmed(false);
    }
}

// Motion Lights (a4): a motion sensor event lights up the room for 60 s
void SmartHome::motionLights(const QString& roomId) {
    if (!autoEnabled("a4")) return;
    auto room = findRoom(roomId);
    if (!room) return;

    QStringList turnedOn;
    for (auto& d : room->devices()) {
        if (d->type() != DeviceType::Light) continue;
        if (!d->isOn()) { d->toggle(); turnedOn << d->id(); }
        d->setValue(100);
    }
    if (turnedOn.isEmpty()) return;

    autoLog(QString("Motion Lights: %1 lights on for 60 s").arg(room->name()));
    emit devicesChanged();
    QTimer::singleShot(60000, this, [this, turnedOn]{
        for (auto& id : turnedOn) {
            auto d = findDevice(id);
            if (d && d->isOn()) d->toggle();
        }
        autoLog("Motion Lights: lights off");
        emit devicesChanged();
    });
}

// Time-based rules (real clock). Returns true if any device state changed.
bool SmartHome::runAutomations(const QDateTime& now) {
    const QDate today = now.date();
    const int mins = now.time().hour() * 60 + now.time().minute();
    bool changed = false;
    auto ranToday = [&](const QString& key){ return m_lastRun.value(key) == today; };

    // a1 Sunrise Wake: 07:00-07:30, living + master lights fade in (steps of 5 min)
    if (autoEnabled("a1") && mins >= 420 && mins < 450) {
        const int target = 10 + ((mins - 420) / 5) * 8;   // 10 .. 58
        for (auto& r : m_rooms) {
            if (r->id() != "master" && r->id() != "living") continue;
            for (auto& d : r->devices()) {
                if (d->type() != DeviceType::Light) continue;
                if (!d->isOn()) { d->toggle(); d->setValue(target); changed = true; }
                else if (d->value() < target) { d->setValue(target); changed = true; }
            }
        }
        if (!ranToday("a1")) {
            m_lastRun["a1"] = today;
            autoLog("Sunrise Wake: lights fading in");
        }
    }

    // a3 Climate Guard: thermostat targets follow the outdoor temperature band
    if (autoEnabled("a3")) {
        const int band = m_outTemp >= 32 ? 3 : m_outTemp >= 26 ? 2 : m_outTemp >= 18 ? 1 : 0;
        if (band != m_climateBand) {
            m_climateBand = band;
            const int target = band == 3 ? 25 : band == 2 ? 24 : band == 1 ? 22 : 21;
            bool any = false;
            for (auto& r : m_rooms) {
                bool hasThermo = false;
                for (auto& d : r->devices())
                    if (d->type() == DeviceType::Thermostat) hasThermo = true;
                if (hasThermo && r->targetTemp() != target) {
                    setRoomTargetTemp(r->id(), target);
                    any = true;
                }
            }
            if (any) {
                autoLog(QString("Climate Guard: outdoor %1°C -> target %2°C")
                        .arg(m_outTemp, 0, 'f', 0).arg(target));
                changed = true;
            }
        }
    } else {
        m_climateBand = -99;
    }

    // a5 Bedtime Routine: 22:30-23:30, once per day, dim every light that is on
    if (autoEnabled("a5") && mins >= 1350 && mins < 1410 && !ranToday("a5")) {
        m_lastRun["a5"] = today;
        for (auto& d : allDevices())
            if (d->type() == DeviceType::Light && d->isOn() && d->value() > 20) {
                d->setValue(20);
                changed = true;
            }
        autoLog("Bedtime Routine: lights dimmed to 20%");
    }

    // a6 Energy Saver: switch smart plugs off during peak hours (17:00-22:00)
    // and back on when off-peak starts
    const bool peak = mins >= 1020 && mins < 1320;
    if (autoEnabled("a6")) {
        if (peak && !ranToday("a6")) {
            m_lastRun["a6"] = today;
            m_energySaverOff.clear();
            for (auto& d : allDevices())
                if (d->type() == DeviceType::SmartPlug && d->isOn()) {
                    d->toggle();
                    m_energySaverOff << d->id();
                }
            if (!m_energySaverOff.isEmpty()) {
                autoLog(QString("Energy Saver: %1 plugs off for peak hours")
                        .arg(m_energySaverOff.size()));
                changed = true;
            }
        }
    }
    if (!peak && !m_energySaverOff.isEmpty()) {
        for (auto& id : m_energySaverOff) {
            auto d = findDevice(id);
            if (d && !d->isOn()) d->toggle();
        }
        autoLog("Energy Saver: off-peak, plugs restored");
        m_energySaverOff.clear();
        changed = true;
    }

    return changed;
}

// ─── Logging ───────────────────────────────────────────────────────────────

void SmartHome::addLog(const QString& room, const QString& msg,
                       LogEntry::Level level) {
    LogEntry e;
    e.timestamp = QDateTime::currentDateTime().toString("hh:mm");
    e.room      = room;
    e.message   = msg;
    e.level     = level;
    m_log.prepend(e);
    if (m_log.size() > 100) m_log.removeLast();
    emit logUpdated();
}

// ─── Outdoor simulation ───────────────────────────────────────────────────

void SmartHome::updateOutdoor(const QDateTime& now) {
    auto* rng = QRandomGenerator::global();
    const double h = now.time().hour() + now.time().minute() / 60.0;

    // Daily curve: ~19°C at 03:00, ~33°C at 15:00, plus a slow random wobble
    m_outNoise = qBound(-1.5, m_outNoise + (rng->generateDouble() - 0.5) * 0.2, 1.5);
    m_outTemp  = 26.0 + 7.0 * qSin(kPi * (h - 9.0) / 12.0) + m_outNoise;
    m_outHum   = qBound(20, qRound(80.0 - (m_outTemp - 19.0) * 2.2), 95);
    m_aqi      = qBound(20, m_aqi + rng->bounded(7) - 3, 160);
    m_uv       = m_solarWatts / 1800.0 * 11.0;
}

// ─── Simulation tick ──────────────────────────────────────────────────────

void SmartHome::onSimulationTick() {
    auto* rng = QRandomGenerator::global();
    const QDateTime now = QDateTime::currentDateTime();

    // Midnight rollover
    if (now.date() != m_lastDate) {
        m_lastDate = now.date();
        m_dailyKwh = 0.0;
        m_solarKwh = 0.0;
        m_hourlyUsage.fill(0.0);
    }

    for (auto& r : m_rooms) {
        bool changed = false;

        QSharedPointer<Device> thermo;
        for (auto& d : r->devices())
            if (d->type() == DeviceType::Thermostat) { thermo = d; break; }

        double t = r->temperature();
        if (thermo && thermo->isOn()) {
            double diff = r->targetTemp() - t;
            if (qAbs(diff) > 0.05) {
                t += qBound(-0.2, diff * 0.15, 0.2);
                changed = true;
            }
        } else if (rng->bounded(10) < 2) {
            t += (rng->generateDouble() - 0.5) * 0.4;
            changed = true;
        }
        if (changed) r->setTemperature(qBound(15.0, t, 35.0));

        if (rng->bounded(20) < 1) {
            r->setHumidity(r->humidity() + rng->bounded(3) - 1);
            changed = true;
        }

        if (changed) emit roomEnvironmentChanged(r->id());
    }
    updateThermostatLoads();

    // Energy: accumulate kWh for this tick into the current hour (kW * hours)
    const int h = now.time().hour();
    const double e = totalWatts() / 1000.0 * (3.0 / 3600.0);
    m_hourlyUsage[h] += e;
    m_dailyKwh       += e;
    m_solarWatts      = solarAt(now.time());
    m_solarKwh       += m_solarWatts / 1000.0 * (3.0 / 3600.0);

    updateOutdoor(now);
    const bool devChanged = runAutomations(now);

    if (devChanged) emit devicesChanged();
    emit statsChanged();
}

// ─── Persistence ──────────────────────────────────────────────────────────

bool SmartHome::saveToFile(const QString& path) const {
    QJsonObject root;
    QJsonArray  roomArr;
    for (auto& r : m_rooms) roomArr.append(r->toJson());
    root["rooms"]          = roomArr;
    root["alarm"]          = m_alarmArmed;
    root["alarmTriggered"] = m_alarmTriggered;
    root["homePresent"]    = m_homePresent;

    // Energy data is only valid for the day it was saved
    root["savedDate"] = m_lastDate.toString(Qt::ISODate);
    root["dailyKwh"]  = m_dailyKwh;
    root["solarKwh"]  = m_solarKwh;
    QJsonArray hourly;
    for (double v : m_hourlyUsage) hourly.append(v);
    root["hourly"] = hourly;

    QJsonArray autoArr;
    for (auto& a : m_automations) {
        QJsonObject o;
        o["id"] = a.id;
        o["enabled"] = a.enabled;
        autoArr.append(o);
    }
    root["automations"] = autoArr;

    QString activeScene;
    for (auto& s : m_scenes) if (s.active) activeScene = s.id;
    root["activeScene"] = activeScene;

    QJsonArray alertArr, logArr;
    for (auto& a : m_alerts) alertArr.append(a.toJson());
    for (auto& l : m_log)    logArr.append(l.toJson());
    root["alerts"] = alertArr;
    root["log"]    = logArr;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    return f.write(QJsonDocument(root).toJson()) != -1;
}

bool SmartHome::loadFromFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;

    // Reject invalid files BEFORE touching any state
    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return false;
    auto root = doc.object();
    if (!root["rooms"].isArray()) return false;

    if (root.contains("alarm")) m_alarmArmed = root["alarm"].toBool();
    m_alarmTriggered = root["alarmTriggered"].toBool();
    if (root.contains("homePresent")) m_homePresent = root["homePresent"].toBool(true);

    for (auto v : root["rooms"].toArray()) {
        auto obj  = v.toObject();
        auto room = findRoom(obj["id"].toString());
        if (room) room->fromJson(obj);
    }
    syncThermostats();
    updateThermostatLoads();

    // Energy history: restore only if it was saved today, otherwise keep today's fresh values
    if (root["savedDate"].toString() == QDate::currentDate().toString(Qt::ISODate)) {
        m_dailyKwh = root["dailyKwh"].toDouble(m_dailyKwh);
        m_solarKwh = root["solarKwh"].toDouble(m_solarKwh);
        auto hourly = root["hourly"].toArray();
        if (hourly.size() == 24)
            for (int i = 0; i < 24; ++i) m_hourlyUsage[i] = hourly[i].toDouble();
    }

    for (auto v : root["automations"].toArray()) {
        auto o = v.toObject();
        for (auto& a : m_automations)
            if (a.id == o["id"].toString()) a.enabled = o["enabled"].toBool();
    }
    m_climateBand = -99;
    m_lastRun.clear();

    const QString activeScene = root["activeScene"].toString();
    for (auto& s : m_scenes) s.active = (!activeScene.isEmpty() && s.id == activeScene);

    if (root["alerts"].isArray()) {
        m_alerts.clear();
        for (auto v : root["alerts"].toArray()) m_alerts.append(LogEntry::fromJson(v.toObject()));
    }
    if (root["log"].isArray()) {
        m_log.clear();
        for (auto v : root["log"].toArray()) m_log.append(LogEntry::fromJson(v.toObject()));
        while (m_log.size() > 100) m_log.removeLast();
    }

    addLog("SYSTEM", "State loaded", LogEntry::Level::Success);
    emit alarmStateChanged(m_alarmArmed);
    emit alertsChanged();
    emit devicesChanged();
    return true;
}

// ─── Default home builder ─────────────────────────────────────────────────

void SmartHome::buildDefaultHome() {
    auto* rng = QRandomGenerator::global();

    struct RoomDef { QString id, name, emoji; };
    QVector<RoomDef> roomDefs = {
        {"living",   "Living Room",  "🛋"},
        {"kitchen",  "Kitchen",      "🍳"},
        {"master",   "Master Bed",   "🛏"},
        {"bathroom", "Bathroom",     "🚿"},
        {"office",   "Home Office",  "💼"},
        {"garden",   "Garden",       "🌿"},
    };

    struct DevDef { QString id, name, icon; DeviceType type; };
    QMap<QString, QVector<DevDef>> devDefs = {
        {"living",  {{"l1","Main Light","💡",DeviceType::Light},
                     {"l2","Accent Lamp","🕯",DeviceType::Light},
                     {"l3","Smart TV","📺",DeviceType::TV},
                     {"l4","Speaker","🔊",DeviceType::Speaker},
                     {"l5","Thermostat","🌡",DeviceType::Thermostat},
                     {"l6","Blinds","🪟",DeviceType::Blinds}}},
        {"kitchen", {{"k1","Ceiling Light","💡",DeviceType::Light},
                     {"k2","Smart Plug","🔌",DeviceType::SmartPlug},
                     {"k3","Vent Fan","🌀",DeviceType::Fan},
                     {"k4","Smoke Sensor","🔴",DeviceType::SmokeSensor}}},
        {"master",  {{"m1","Ambient Light","💡",DeviceType::Light},
                     {"m2","Ceiling Fan","🌀",DeviceType::Fan},
                     {"m3","Thermostat","🌡",DeviceType::Thermostat},
                     {"m4","Smart Lock","🔐",DeviceType::Lock}}},
        {"bathroom",{{"b1","Mirror Light","💡",DeviceType::Light},
                     {"b2","Exhaust Fan","🌀",DeviceType::Fan},
                     {"b3","Heater","♨",DeviceType::SmartPlug}}},
        {"office",  {{"o1","Desk Light","💡",DeviceType::Light},
                     {"o2","Monitor","🖥",DeviceType::SmartPlug},
                     {"o3","Camera","📷",DeviceType::Camera},
                     {"o4","Speaker","🔊",DeviceType::Speaker}}},
        {"garden",  {{"g1","Garden Light","💡",DeviceType::Light},
                     {"g2","Irrigation","💧",DeviceType::SmartPlug},
                     {"g3","Camera","📷",DeviceType::Camera},
                     {"g4","Motion Sensor","👁",DeviceType::Sensor}}},
    };

    for (auto& rd : roomDefs) {
        auto room = QSharedPointer<Room>::create(rd.id, rd.name, rd.emoji);
        room->setTemperature(21 + rng->bounded(4));
        room->setHumidity(40 + rng->bounded(25));

        for (auto& dd : devDefs.value(rd.id)) {
            auto dev = QSharedPointer<Device>::create(dd.id, dd.name, dd.icon, dd.type, rd.id);

            const bool alwaysOn = dd.type == DeviceType::Lock        ||
                                  dd.type == DeviceType::Sensor      ||
                                  dd.type == DeviceType::SmokeSensor ||
                                  dd.type == DeviceType::Camera;
            if (alwaysOn || rng->bounded(100) > 45) dev->toggle();

            switch (dd.type) {
                case DeviceType::Thermostat: dev->setValue(19 + rng->bounded(6)); break;
                case DeviceType::Fan:        dev->setValue(1 + rng->bounded(3));  break;
                case DeviceType::Lock:       dev->setValue(100);                  break;
                default:                     dev->setValue(50 + rng->bounded(50)); break;
            }
            room->addDevice(dev);
        }
        m_rooms.append(room);
        addLog(rd.name, QString("%1 devices active").arg(room->activeDeviceCount()));
    }
    syncThermostats();
    updateThermostatLoads();
}

void SmartHome::buildScenes() {
    m_scenes = {
        {"morning", "Morning",  "🌅", "Gentle wake-up routine"},
        {"cinema",  "Cinema",   "🎬", "Movie time ambience"},
        {"dinner",  "Dinner",   "🍽", "Dinner table lighting"},
        {"away",    "Away",     "🚗", "Nobody home mode"},
        {"night",   "Night",    "🌙", "Sleep mode"},
        {"party",   "Party",    "🎉", "Let's go!"},
    };
}

void SmartHome::buildAutomations() {
    m_automations = {
        {"a1","Sunrise Wake",   "🌅","Lights on at 7:00 AM gradually",           true },
        {"a2","Departure Mode", "🚗","All off when phone leaves home",            true },
        {"a3","Climate Guard",  "🌡","Thermostat adjusts to outside temperature", true },
        {"a4","Motion Lights",  "💡","Hallway lights on motion detection",        false},
        {"a5","Bedtime Routine","🌙","Dim all lights at 10:30 PM",                true },
        {"a6","Energy Saver",   "⚡","Off-peak appliance scheduling",             false},
    };
}
