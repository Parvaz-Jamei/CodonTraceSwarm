#include <Arduino.h>
#include <Wire.h>
#include <CodonTraceSwarm.h>
#include <cts/SensorSample.h>
#include <Adafruit_BME280.h>

// A library without its own adapter. Copy finite numbers into samples.
static cts::Device device;
static char message[cts::TxBytes];
static Adafruit_BME280 chip;
static const uint64_t kNotAClockMs = 1;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  chip.begin(0x76);
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
  session.wifi = true;
  device.define(temperature);
  device.define(humidity);
  device.begin(session);
  Serial.println("Any library: pass its numbers to scalarIfFinite. Not sent. Not a bench result. Timestamp is not a clock.");
}
void loop() {
  const double temperatureFromLibrary = chip.readTemperature();
  const double humidityFromLibrary = chip.readHumidity();
  cts::Sample temperature = cts::scalarIfFinite(temperatureFromLibrary, kNotAClockMs);
  cts::Sample humidity = cts::scalarIfFinite(humidityFromLibrary, kNotAClockMs);
  cts::Error temperatureEncoded =
      device.reading("temperature", temperature, message, sizeof message);
  cts::Error humidityEncoded =
      device.reading("humidity", humidity, message, sizeof message);
  Serial.print("any temperature valid=");
  Serial.print(temperature.valid ? "true" : "false");
  Serial.print(" encode=");
  Serial.print(cts::errorName(temperatureEncoded));
  Serial.print(" humidity valid=");
  Serial.print(humidity.valid ? "true" : "false");
  Serial.print(" encode=");
  Serial.println(cts::errorName(humidityEncoded));
  delay(3000);
}
