// HomeP1Device.cpp - CORRECTED VERSION
#include "HomeP1Device.h"

HomeP1Device::HomeP1Device(const char *ip)
    : baseUrl("http://" + String(ip)), lastImportPower(0), lastExportPower(0),
      lastTotalImport(0), lastTotalExport(0), lastReadTime(0),
      lastReadSuccess(false) {
  Serial.printf("P1 meter initialized at: %s\n", ip);
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
  // Create LOCAL instances (not persistent)
  WiFiClient localClient;
  HTTPClient localHttp;

  // Configure timeouts
  localClient.setTimeout(8000); // 8 seconds for P1 meter
  localHttp.setTimeout(8000);
  // NO setConnectTimeout() - causes socket exhaustion!
  localHttp.setReuse(false);

  Serial.printf("P1 > Fetching from: %s/api/v1/data\n", baseUrl.c_str());

  if (!localHttp.begin(localClient, baseUrl + "/api/v1/data")) {
    Serial.println("P1 > HTTP begin failed!");
    localClient.stop();
    delay(50);
    return false;
  }

  int httpCode = localHttp.GET();
  Serial.printf("P1 > HTTP code: %d\n", httpCode);

  if (httpCode == HTTP_CODE_OK) {
    String payload = localHttp.getString();
    Serial.printf("P1 > Payload length: %d\n", payload.length());

    StaticJsonDocument<1536> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.printf("P1 > JSON parse error: %s\n", error.c_str());
      localHttp.end();
      localClient.stop();
      delay(100);
      return false;
    }

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

    localHttp.end();
    localClient.stop();
    delay(100); // CRITICAL: Let socket close properly
    return true;

  } else {
    Serial.printf("P1 > HTTP error: %d\n", httpCode);
  }

  localHttp.end();
  localClient.stop();
  delay(100); // CRITICAL: Let socket close properly
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