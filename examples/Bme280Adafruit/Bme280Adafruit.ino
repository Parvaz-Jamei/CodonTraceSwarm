#include <Arduino.h>
#include <Wire.h>
#include <CodonTraceSwarm.h>
#include <cts/sensors/Bme280Adafruit.h>

static cts::Device device;
static char message[cts::TxBytes];
static Adafruit_BME280 chip;
static const uint64_t kNotAClockMs = 1;

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
  const bool opened = chip.begin(0x76);
  cts::SessionConfig session;
  session.swarmId = "sensor";
  session.nodeId = "node";
  session.bootId = "bme280-adafruit";
  session.wifi = true;
  const bool defined =
      device.define(channel("temperature", "temperature", "degC")) ==
          cts::Error::Ok &&
      device.define(channel("pressure", "pressure", "Pa")) == cts::Error::Ok &&
      device.define(channel("humidity", "humidity", "percent")) ==
          cts::Error::Ok &&
      device.begin(session) == cts::Error::Ok;
  Serial.println(defined ? "codec ready" : "codec failed");
  Serial.println(opened ? "Adafruit BME280 begin returned true. Not a bench result."
                        : "Adafruit BME280 begin returned false.");
}
void loop() {
  cts::Sample temperature = cts::bme280Temperature(chip, kNotAClockMs);
  cts::Sample pressure = cts::bme280PressurePa(chip, kNotAClockMs);
  cts::Sample humidity = cts::bme280Humidity(chip, kNotAClockMs);
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
