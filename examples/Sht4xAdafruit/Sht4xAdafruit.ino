#include <Arduino.h>
#include <Wire.h>
#include <CodonTraceSwarm.h>
#include <cts/sensors/Sht4xAdafruit.h>

static cts::Device device;
static char message[cts::TxBytes];
static Adafruit_SHT4x chip;
static const uint64_t kNotAClockMs = 1;

static cts::ChannelSpec channel(const char *name, const char *quantity,
                                const char *unit) {
  cts::ChannelSpec spec;
  spec.name = name;
  spec.quantity = quantity;
  spec.unit = unit;
  spec.descriptor.featureId = "sht4x";
  spec.descriptor.configId = "adafruit";
  spec.descriptor.sourceUnit = unit;
  return spec;
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  const bool opened = chip.begin();
  cts::SessionConfig session;
  session.swarmId = "sensor";
  session.nodeId = "node";
  session.bootId = "sht4x-adafruit";
  session.wifi = true;
  const bool defined =
      device.define(channel("temperature", "temperature", "degC")) ==
          cts::Error::Ok &&
      device.define(channel("humidity", "humidity", "percent")) ==
          cts::Error::Ok &&
      device.begin(session) == cts::Error::Ok;
  Serial.println(defined ? "codec ready" : "codec failed");
  Serial.println(opened ? "Adafruit SHT4x begin returned true. Not a bench result."
                        : "Adafruit SHT4x begin returned false.");
  Serial.println("SHT4x pressure is unsupported. Not sent.");
}
void loop() {
  cts::Sample temperature = cts::sht4xTemperature(chip, kNotAClockMs);
  cts::Sample humidity = cts::sht4xHumidity(chip, kNotAClockMs);
  cts::Error temperatureEncoded =
      device.reading("temperature", temperature, message, sizeof message);
  cts::Error humidityEncoded =
      device.reading("humidity", humidity, message, sizeof message);
  Serial.print("sht4x temperature valid=");
  Serial.print(temperature.valid ? "true" : "false");
  Serial.print(" encode=");
  Serial.print(cts::errorName(temperatureEncoded));
  Serial.print(" humidity valid=");
  Serial.print(humidity.valid ? "true" : "false");
  Serial.print(" encode=");
  Serial.print(cts::errorName(humidityEncoded));
  Serial.println(". Local JSON only. Not sent. Not a bench result.");
  delay(3000);
}
