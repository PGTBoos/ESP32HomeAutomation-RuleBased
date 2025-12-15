// EnvironmentSensors.cpp
#include "EnvironmentSensor.h"

bool EnvironmentSensors::begin() {
  Wire.begin(); // Start I2C

  // Initialize BME280
  bmeFound = bme.begin(0x76); // Try first address
  if (!bmeFound) {
    bmeFound = bme.begin(0x77); // Try alternate address
  }
  if (!bmeFound) {
    Serial.println("Could not find BME280 sensor!");
  }

  // Initialize BH1750
  lightMeterFound = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  if (!lightMeterFound) {
    Serial.println("Could not find BH1750 sensor!");
  } else {
    lightMeter.setMTreg(64);     // Set measurement time to 400ms
    lightMeter.readLightLevel(); // Read once to start measurement

    Serial.println("BH1750 sensor found!");
  }

  return bmeFound ||
         lightMeterFound; // Return true if at least one sensor works
}

void EnvironmentSensors::update() {
  if (bmeFound) {
    temperature = bme.readTemperature();
    humidity = bme.readHumidity();
    pressure = bme.readPressure() / 100.0F; // Convert to hPa
  }

  if (lightMeterFound) {
    lightLevel = lightMeter.readLightLevel();
  }
}

// Getter methods
float EnvironmentSensors::getTemperature() const {
  return temperature;
}

float EnvironmentSensors::getHumidity() const {
  return humidity;
}

float EnvironmentSensors::getPressure() const {
  return pressure;
}

float EnvironmentSensors::getLightLevel() const {
  return lightLevel;
}

// Status methods
bool EnvironmentSensors::hasBME280() const {
  return bmeFound;
}

bool EnvironmentSensors::hasBH1750() const {
  return lightMeterFound;
}