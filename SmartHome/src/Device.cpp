#include "Device.h"
#include <QJsonObject>

Device::Device(const QString& id, const QString& name, const QString& icon,
               DeviceType type, const QString& roomId)
    : m_id(id), m_name(name), m_icon(icon), m_type(type), m_roomId(roomId)
{
}

void Device::toggle() {
    m_on = !m_on;
}

void Device::setValue(int v) {
    if      (m_type == DeviceType::Thermostat) m_value = qBound(16, v, 30);
    else if (m_type == DeviceType::Fan)        m_value = qBound(1,  v, 3);
    else                                       m_value = qBound(1,  v, 100);
}

double Device::basePower() const {
    switch (m_type) {
        case DeviceType::Light:       return 8.0;
        case DeviceType::Thermostat:  return 1500.0;
        case DeviceType::Lock:        return 2.0;
        case DeviceType::Camera:      return 12.0;
        case DeviceType::TV:          return 120.0;
        case DeviceType::Fan:         return 80.0;
        case DeviceType::SmartPlug:   return 200.0;
        case DeviceType::Sensor:      return 1.0;
        case DeviceType::SmokeSensor: return 1.0;
        case DeviceType::Speaker:     return 40.0;
        case DeviceType::Blinds:      return 30.0;
        default:                      return 20.0;
    }
}

// FIX: value means different things per type, so power is computed per type.
//  - Light: brightness percentage
//  - Fan:   speed 1..3
//  - Thermostat: its value is a temperature, so draw follows the duty factor
//    SmartHome sets from |room temp - target| (idle ~5%, full when far off)
//  - others: constant draw
double Device::currentWatts() const {
    if (!m_on) return 0.0;
    switch (m_type) {
        case DeviceType::Light:      return basePower() * (m_value / 100.0);
        case DeviceType::Fan:        return basePower() * (m_value / 3.0);
        case DeviceType::Thermostat: return basePower() * m_load;
        default:                return basePower();
    }
}

QString Device::statusText() const {
    // FIX: a lock that is off is "Unlocked", not "OFF"
    if (!m_on) return m_type == DeviceType::Lock ? "Unlocked" : "OFF";
    if (m_alert) return "ALERT!";
    switch (m_type) {
        case DeviceType::Light:      return QString("%1%").arg(m_value);
        case DeviceType::Thermostat: return QString("%1°C").arg(m_value);
        case DeviceType::Fan:        return QString("Speed %1").arg(m_value);
        case DeviceType::TV:         return "Playing";
        case DeviceType::Lock:       return "Locked";
        case DeviceType::Camera:     return "Active";
        case DeviceType::Sensor:     return "Armed";
        case DeviceType::SmokeSensor:return "Armed";
        default:                     return "ON";
    }
}

QString Device::typeString() const {
    switch (m_type) {
        case DeviceType::Light:       return "Light";
        case DeviceType::Thermostat:  return "Thermostat";
        case DeviceType::Lock:        return "Lock";
        case DeviceType::Camera:      return "Camera";
        case DeviceType::TV:          return "TV";
        case DeviceType::Fan:         return "Fan";
        case DeviceType::SmartPlug:   return "Smart Plug";
        case DeviceType::Sensor:      return "Motion Sensor";
        case DeviceType::SmokeSensor: return "Smoke Sensor";
        case DeviceType::Speaker:     return "Speaker";
        case DeviceType::Blinds:      return "Blinds";
        default:                      return "Device";
    }
}

QJsonObject Device::toJson() const {
    QJsonObject obj;
    obj["id"]     = m_id;
    obj["name"]   = m_name;
    obj["icon"]   = m_icon;
    obj["type"]   = static_cast<int>(m_type);
    obj["roomId"] = m_roomId;
    obj["on"]     = m_on;
    obj["value"]  = m_value;
    obj["alert"]  = m_alert;
    return obj;
}

void Device::fromJson(const QJsonObject& obj) {
    m_on    = obj["on"].toBool();
    setValue(obj["value"].toInt(100));   // FIX: clamp loaded value to valid range
    m_alert = obj["alert"].toBool();
}
