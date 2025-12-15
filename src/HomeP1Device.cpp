// HomeP1Device.cpp
#include "HomeP1Device.h"

HomeP1Device::HomeP1Device(const char *ip)
    : baseUrl("http://" + String(ip)), lastImportPower(0), lastExportPower(0),
      lastTotalImport(0), lastTotalExport(0), lastReadTime(0),
      lastReadSuccess(false) {
}
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

void HomeP1Device::update() {
  if (millis() - lastReadTime >= READ_INTERVAL) {
    lastReadSuccess = getPowerData(lastImportPower, lastExportPower);
    lastReadTime = millis();
  }
}

bool HomeP1Device::getPowerData(float &importPower, float &exportPower) {
  http.begin(client, baseUrl + "/api/v1/data");
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    StaticJsonDocument<1536> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      float power = doc["active_power_w"].as<float>();
      lastTotalImport = doc["total_power_import_kwh"].as<float>();
      lastTotalExport = doc["total_power_export_kwh"].as<float>();

      Serial.printf("Received P1 power data: %.2f W\n", power);
      Serial.printf("Today total import: %.2f kWh\n", lastTotalImport);
      Serial.printf("Today total export: %.2f kWh\n", lastTotalExport);

      importPower = max(power, 0);
      exportPower = max(-power, 0);

      lastImportPower = importPower;
      lastExportPower = exportPower;

      http.end();
      return true;
    }
  }
  http.end();
  return false;
}

float HomeP1Device::getCurrentImport() const {
  return lastImportPower;
}

float HomeP1Device::getCurrentExport() const {
  return lastExportPower;
}

float HomeP1Device::getTotalImport() const {
  return lastTotalImport;
}

float HomeP1Device::getTotalExport() const {
  return lastTotalExport;
}

float HomeP1Device::getNetPower() const {
  return lastImportPower - lastExportPower;
}

bool HomeP1Device::isConnected() const {
  return lastReadSuccess;
}