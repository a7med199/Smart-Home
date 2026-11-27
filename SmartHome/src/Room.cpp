#include "Room.h"
#include <QJsonArray>
#include <algorithm>

Room::Room(const QString& id, const QString& name,
           const QString& emoji, QObject* parent)
    : QObject(parent), m_id(id), m_name(name), m_emoji(emoji)
{
}

void Room::setTemperature(double t) {
    m_temperature = t;
    emit temperatureChanged(t);
}

void Room::setHumidity(int h) {
    m_humidity = qBound(0, h, 100);
    emit humidityChanged(m_humidity);
}

void Room::setTargetTemp(int t) {
    m_targetTemp = qBound(16, t, 30);
}

void Room::addDevice(QSharedPointer<Device> dev) {
    m_devices.append(dev);
}

void Room::removeDevice(const QString& deviceId) {
    m_devices.erase(
        std::remove_if(m_devices.begin(), m_devices.end(),
                       [&](const QSharedPointer<Device>& d){ return d->id() == deviceId; }),
        m_devices.end());
}

QSharedPointer<Device> Room::findDevice(const QString& deviceId) const {
    for (auto& d : m_devices)
        if (d->id() == deviceId) return d;
    return nullptr;
}

int Room::activeDeviceCount() const {
    int count = 0;
    for (auto& d : m_devices) if (d->isOn()) ++count;
    return count;
}

double Room::totalWatts() const {
    double total = 0;
    for (auto& d : m_devices) total += d->currentWatts();
    return total;
}

QJsonObject Room::toJson() const {
    QJsonObject obj;
    obj["id"]          = m_id;
    obj["name"]        = m_name;
    obj["emoji"]       = m_emoji;
    obj["temperature"] = m_temperature;
    obj["humidity"]    = m_humidity;
    obj["targetTemp"]  = m_targetTemp;
    QJsonArray devArr;
    for (auto& d : m_devices) devArr.append(d->toJson());
    obj["devices"] = devArr;
    return obj;
}

void Room::fromJson(const QJsonObject& obj) {
    m_temperature = obj["temperature"].toDouble(21.0);
    m_humidity    = obj["humidity"].toInt(50);
    m_targetTemp  = obj["targetTemp"].toInt(22);
    // FIX: iterate devices first, then find matching JSON entry
    auto arr = obj["devices"].toArray();
    for (auto& d : m_devices) {
        for (auto v : arr) {
            auto devObj = v.toObject();
            if (devObj["id"].toString() == d->id()) {
                d->fromJson(devObj);
                break; // FIX: stop after first match to avoid redundant iterations
            }
        }
    }
}
