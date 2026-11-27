#pragma once
#include <QString>
#include <QJsonObject>

enum class DeviceType {
    Light,
    Thermostat,
    Lock,
    Camera,
    TV,
    Fan,
    SmartPlug,
    Sensor,       // motion sensor
    Speaker,
    Blinds,
    SmokeSensor   // appended last so saved JSON "type" ints stay valid
};

class Device {
public:
    Device(const QString& id, const QString& name, const QString& icon,
           DeviceType type, const QString& roomId);
    virtual ~Device() = default;

    // Getters
    QString     id()     const { return m_id; }
    QString     name()   const { return m_name; }
    QString     icon()   const { return m_icon; }
    DeviceType  type()   const { return m_type; }
    QString     roomId() const { return m_roomId; }
    bool        isOn()   const { return m_on; }
    int         value()  const { return m_value; }
    bool        hasAlert() const { return m_alert; }

    // Control
    virtual void toggle();
    virtual void setValue(int v);
    virtual void setAlert(bool a) { m_alert = a; }
    // 0..1 duty factor (used by thermostats: how hard they are heating/cooling)
    void   setLoadFactor(double f) { m_load = qBound(0.0, f, 1.0); }
    double loadFactor() const      { return m_load; }

    // Power
    virtual double currentWatts() const;
    virtual double basePower()    const;

    // Display
    virtual QString statusText() const;
    virtual QString typeString() const;

    // Serialisation
    virtual QJsonObject toJson()  const;
    virtual void fromJson(const QJsonObject& obj);

protected:
    QString    m_id;
    QString    m_name;
    QString    m_icon;
    DeviceType m_type;
    QString    m_roomId;
    bool       m_on    = false;
    int        m_value = 100;
    bool       m_alert = false;
    double     m_load  = 1.0;
};
