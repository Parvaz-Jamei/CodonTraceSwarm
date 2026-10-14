#include <Arduino.h>
#include <Wire.h>
#include <CodonTraceSwarm.h>
#include <cts/sensors/Sht4xAdafruit.h>

static cts::Device device;
static char message[cts::TxBytes];
static Adafruit_SHT4x chip;
static bool chipOpened = false;
static bool codecReady = false;

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
  chipOpened = chip.begin();
  cts::SessionConfig session;
  session.swarmId = "sensor";
  session.nodeId = "node";
  session.bootId = "sht4x-adafruit";
  session.wifi = false;
  codecReady =
      device.define(channel("temperature", "temperature", "degC")) ==
          cts::Error::Ok &&
      device.define(channel("humidity", "humidity", "percent")) ==
          cts::Error::Ok &&
      device.begin(session) == cts::Error::Ok;
  Serial.println(codecReady ? "codec ready" : "codec failed");
  Serial.println(chipOpened ? "Adafruit SHT4x begin returned true. Not a bench result. Not flashed."
                            : "Adafruit SHT4x begin returned false. Getter not called for this boot.");
  Serial.println("SHT4x pressure is unsupported. Not sent. Timestamp is not a clock.");
}
void loop() {
  if (!chipOpened) {
    Serial.println("SHT4x begin failed. Getter not called. Sample skipped. Timestamp is not a clock.");
    delay(3000);
    return;
  }
  cts::Sample temperature = cts::sht4xTemperature(chip, 0);
  cts::Sample humidity = cts::sht4xHumidity(chip, 0);
  Serial.print("temperature=");
  Serial.print(temperature.value);
  Serial.print(" degC humidity=");
  Serial.print(humidity.value);
  Serial.print(" percent");
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
  Serial.println(". Local JSON only. The line was not sent. Not a bench result. Timestamp is not a clock.");
  delay(3000);
}
