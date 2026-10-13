#include <Arduino.h>
#include <Wire.h>
#include <CodonTraceSwarm.h>
#include <cts/sensors/Ina219Adafruit.h>

static cts::Device device;
static char message[cts::TxBytes];
static Adafruit_INA219 chip;
static const uint64_t kNotAClockMs = 1;

static cts::ChannelSpec channel(const char *name, const char *quantity,
                                const char *unit) {
  cts::ChannelSpec spec;
  spec.name = name;
  spec.quantity = quantity;
  spec.unit = unit;
  spec.descriptor.featureId = "ina219";
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
  session.bootId = "ina219-adafruit";
  session.wifi = true;
  const bool defined =
      device.define(channel("shunt", "voltage", "mV")) == cts::Error::Ok &&
      device.define(channel("bus", "voltage", "V")) == cts::Error::Ok &&
      device.define(channel("current", "current", "mA")) == cts::Error::Ok &&
      device.begin(session) == cts::Error::Ok;
  Serial.println(defined ? "codec ready" : "codec failed");
  Serial.println(opened ? "Adafruit INA219 begin returned true. Not a bench result."
                        : "Adafruit INA219 begin returned false.");
}
void loop() {
  cts::Sample shunt = cts::ina219ShuntMv(chip, kNotAClockMs);
  cts::Sample bus = cts::ina219BusV(chip, kNotAClockMs);
  cts::Sample current = cts::ina219CurrentMa(chip, kNotAClockMs);
  cts::Error shuntEncoded =
      device.reading("shunt", shunt, message, sizeof message);
  cts::Error busEncoded = device.reading("bus", bus, message, sizeof message);
  cts::Error currentEncoded =
      device.reading("current", current, message, sizeof message);
  Serial.print("ina219 shunt valid=");
  Serial.print(shunt.valid ? "true" : "false");
  Serial.print(" encode=");
  Serial.print(cts::errorName(shuntEncoded));
  Serial.print(" bus valid=");
  Serial.print(bus.valid ? "true" : "false");
  Serial.print(" encode=");
  Serial.print(cts::errorName(busEncoded));
  Serial.print(" current valid=");
  Serial.print(current.valid ? "true" : "false");
  Serial.print(" encode=");
  Serial.print(cts::errorName(currentEncoded));
  Serial.println(". Local JSON only. Not sent. Not a bench result.");
  delay(3000);
}
