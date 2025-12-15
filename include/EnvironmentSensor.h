// EnvironmentSensor.h
#ifndef ENVIRONMENT_SENSORS_H
#define ENVIRONMENT_SENSORS_H

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>

class EnvironmentSensors
{
private:
    Adafruit_BME280 bme;
    BH1750 lightMeter;
    bool bmeFound = false;
    bool lightMeterFound = false;

    float temperature = 0;
    float humidity = 0;
    float pressure = 0;
    float lightLevel = 0;

public:
    bool begin();
    void update();
    float getTemperature() const;
    float getHumidity() const;
    float getPressure() const;
    float getLightLevel() const;
    bool hasBME280() const;
    bool hasBH1750() const;
};

#endif