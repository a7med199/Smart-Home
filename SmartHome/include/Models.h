#pragma once
#include <QString>
#include <QJsonObject>

// ─── Scene ───────────────────────────────────────────────────────────────────
struct Scene {
    QString id;
    QString name;
    QString icon;
    QString description;
    bool    active = false;

    QJsonObject toJson() const {
        QJsonObject o;
        o["id"] = id; o["name"] = name;
        o["icon"] = icon; o["description"] = description;
        o["active"] = active;
        return o;
    }
};

// ─── Automation ──────────────────────────────────────────────────────────────
struct Automation {
    QString id;
    QString name;
    QString icon;
    QString description;
    bool    enabled = false;

    QJsonObject toJson() const {
        QJsonObject o;
        o["id"] = id; o["name"] = name;
        o["icon"] = icon; o["description"] = description;
        o["enabled"] = enabled;
        return o;
    }
};

// ─── LogEntry ─────────────────────────────────────────────────────────────────
struct LogEntry {
    QString timestamp;
    QString room;
    QString message;
    QString deviceId;   // device this entry/alert is tied to (may be empty)
    enum class Level { Info, Warning, Danger, Success } level = Level::Info;

    QJsonObject toJson() const {
        QJsonObject o;
        o["t"] = timestamp; o["room"] = room; o["msg"] = message;
        o["dev"] = deviceId; o["level"] = static_cast<int>(level);
        return o;
    }
    static LogEntry fromJson(const QJsonObject& o) {
        LogEntry e;
        e.timestamp = o["t"].toString();
        e.room      = o["room"].toString();
        e.message   = o["msg"].toString();
        e.deviceId  = o["dev"].toString();
        int l = o["level"].toInt();
        if (l < 0 || l > 3) l = 0;
        e.level = static_cast<Level>(l);
        return e;
    }
};
