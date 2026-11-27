#pragma once
#include "Device.h"
#include <QObject>
#include <QVector>
#include <QSharedPointer>
#include <QJsonObject>

class Room : public QObject {
    Q_OBJECT

public:
    explicit Room(const QString& id, const QString& name,
                  const QString& emoji, QObject* parent = nullptr);
    ~Room() override = default;

    // Identity
    QString id()    const { return m_id; }
    QString name()  const { return m_name; }
    QString emoji() const { return m_emoji; }

    // Sensors
    double temperature() const { return m_temperature; }
    int    humidity()    const { return m_humidity; }
    int    targetTemp()  const { return m_targetTemp; }
    void   setTemperature(double t);
    void   setHumidity(int h);
    void   setTargetTemp(int t);

    // Devices
    void   addDevice(QSharedPointer<Device> dev);
    void   removeDevice(const QString& deviceId);
    QSharedPointer<Device> findDevice(const QString& deviceId) const;
    const QVector<QSharedPointer<Device>>& devices() const { return m_devices; }

    // Aggregates
    int    activeDeviceCount() const;
    double totalWatts()        const;

    // Serialisation
    QJsonObject toJson()  const;
    void        fromJson(const QJsonObject& obj);

signals:
    void temperatureChanged(double newTemp);
    void humidityChanged(int    newHum);
    // FIX: removed the never-emitted deviceToggled signal

private:
    QString  m_id;
    QString  m_name;
    QString  m_emoji;
    double   m_temperature = 21.0;
    int      m_humidity    = 50;
    int      m_targetTemp  = 22;

    QVector<QSharedPointer<Device>> m_devices;
};
