#include <Arduino.h>
#include <Wire.h>
#include <CodonTraceSwarm.h>
#include <cts/SensorSample.h>
#include <Adafruit_BME280.h>

// A library without its own adapter. Copy finite numbers into samples.
// Does not include both FDC1004 wrappers.
static cts::Device device;
static char message[cts::TxBytes];
static Adafruit_BME280 chip;
static bool chipOpened = false;
static bool codecReady = false;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  chipOpened = chip.begin(0x76);
  cts::ChannelSpec temperature;
  temperature.name = "temperature";
  temperature.quantity = "temperature";
  temperature.unit = "degC";
  temperature.descriptor.featureId = "other-library";
  temperature.descriptor.configId = "caller";
  temperature.descriptor.sourceUnit = "degC";
  cts::ChannelSpec humidity;
  humidity.name = "humidity";
  humidity.quantity = "humidity";
  humidity.unit = "%RH";
  humidity.descriptor.featureId = "other-library";
  humidity.descriptor.configId = "caller";
  humidity.descriptor.sourceUnit = "%RH";
  cts::SessionConfig session;
  session.swarmId = "sensor";
  session.nodeId = "node";
  session.bootId = "any-library";
  session.wifi = false;
  codecReady = device.define(temperature) == cts::Error::Ok &&
               device.define(humidity) == cts::Error::Ok &&
               device.begin(session) == cts::Error::Ok;
  Serial.println(codecReady ? "codec ready" : "codec failed");
  Serial.println(chipOpened ? "Any library begin returned true. Not a bench result. Not flashed."
                            : "Any library begin returned false. Getters not called for this boot.");
  Serial.println("Pass finite numbers to scalarIfFinite. Not sent. Timestamp is not a clock.");
}
void loop() {
  if (!chipOpened) {
    Serial.println("begin failed. Getters not called. Sample skipped. Timestamp is not a clock.");
    delay(3000);
    return;
  }
  const double temperatureFromLibrary = chip.readTemperature();
  const double humidityFromLibrary = chip.readHumidity();
  cts::Sample temperature = cts::scalarIfFinite(temperatureFromLibrary, 0);
  cts::Sample humidity = cts::scalarIfFinite(humidityFromLibrary, 0);
  Serial.print("temperature=");
  Serial.print(temperature.value);
  Serial.print(" degC humidity=");
  Serial.print(humidity.value);
  Serial.print(" %RH");
  if (!codecReady) {
    Serial.println(". device define or begin failed. reading() not called. The line was not sent. Timestamp is not a clock.");
    delay(3000);
    return;
  }
  cts::Error temperatureEncoded =
      device.reading("temperature", temperature, message, sizeof message);
  cts::Error humidityEncoded =
      device.reading("humidity", humidity, message, sizeof message);
  Serial.print(" encode temperature=");
  Serial.print(cts::errorName(temperatureEncoded));
  Serial.print(" humidity=");
  Serial.print(cts::errorName(humidityEncoded));
  Serial.println(". The line was not sent. Timestamp is not a clock.");
  delay(3000);
}
