#include <Arduino.h>
#include <Wire.h>
#include <CodonTraceSwarm.h>
#include <cts/sensors/Bme280Adafruit.h>

static cts::Device device;
static char message[cts::TxBytes];
static Adafruit_BME280 chip;
static bool chipOpened = false;
static bool codecReady = false;

static cts::ChannelSpec channel(const char *name, const char *quantity,
                                const char *unit) {
  cts::ChannelSpec spec;
  spec.name = name;
  spec.quantity = quantity;
  spec.unit = unit;
  spec.descriptor.featureId = "bme280";
  spec.descriptor.configId = "adafruit";
  spec.descriptor.sourceUnit = unit;
  return spec;
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  chipOpened = chip.begin(0x76);
  cts::SessionConfig session;
  session.swarmId = "sensor";
  session.nodeId = "node";
  session.bootId = "bme280-adafruit";
  session.wifi = false;
  codecReady =
      device.define(channel("temperature", "temperature", "degC")) ==
          cts::Error::Ok &&
      device.define(channel("pressure", "pressure", "Pa")) == cts::Error::Ok &&
      device.define(channel("humidity", "humidity", "percent")) ==
          cts::Error::Ok &&
      device.begin(session) == cts::Error::Ok;
  Serial.println(codecReady ? "codec ready" : "codec failed");
  Serial.println(chipOpened ? "Adafruit BME280 begin returned true. Not a bench result. Not flashed."
                            : "Adafruit BME280 begin returned false. Getter not called for this boot.");
  Serial.println("Timestamp is not a clock.");
}
void loop() {
  if (!chipOpened) {
    Serial.println("BME280 begin failed. Getter not called. Sample skipped. Timestamp is not a clock.");
    delay(3000);
    return;
  }
  // 0 is not a clock and is not stored as UTC.
  cts::Sample temperature = cts::bme280Temperature(chip, 0);
  cts::Sample pressure = cts::bme280PressurePa(chip, 0);
  cts::Sample humidity = cts::bme280Humidity(chip, 0);
  if (!codecReady) {
    Serial.println("device define or begin failed. reading() not called. Sample skipped. Timestamp is not a clock.");
    delay(3000);
    return;
  }
  cts::Error temperatureEncoded =
      device.reading("temperature", temperature, message, sizeof message);
  cts::Error pressureEncoded =
      device.reading("pressure", pressure, message, sizeof message);
  cts::Error humidityEncoded =
      device.reading("humidity", humidity, message, sizeof message);
  Serial.print("bme280 temperature valid=");
  Serial.print(temperature.valid ? "true" : "false");
  Serial.print(" encode=");
  Serial.print(cts::errorName(temperatureEncoded));
  Serial.print(" pressure valid=");
  Serial.print(pressure.valid ? "true" : "false");
  Serial.print(" encode=");
  Serial.print(cts::errorName(pressureEncoded));
  Serial.print(" humidity valid=");
  Serial.print(humidity.valid ? "true" : "false");
  Serial.print(" encode=");
  Serial.print(cts::errorName(humidityEncoded));
  Serial.println(". Local JSON only. Not sent. Timestamp is not a clock.");
  delay(3000);
}
