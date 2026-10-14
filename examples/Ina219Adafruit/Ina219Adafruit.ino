#include <Arduino.h>
#include <Wire.h>
#include <CodonTraceSwarm.h>
#include <cts/sensors/Ina219Adafruit.h>

static cts::Device device;
static char message[cts::TxBytes];
static Adafruit_INA219 chip;
static bool chipOpened = false;
static bool codecReady = false;

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
  chipOpened = chip.begin();
  cts::SessionConfig session;
  session.swarmId = "sensor";
  session.nodeId = "node";
  session.bootId = "ina219-adafruit";
  session.wifi = false;
  codecReady =
      device.define(channel("shunt", "voltage", "mV")) == cts::Error::Ok &&
      device.define(channel("bus", "voltage", "V")) == cts::Error::Ok &&
      device.define(channel("current", "current", "mA")) == cts::Error::Ok &&
      device.begin(session) == cts::Error::Ok;
  Serial.println(codecReady ? "codec ready" : "codec failed");
  Serial.println(chipOpened ? "Adafruit INA219 begin returned true. Not a bench result. Not flashed."
                            : "Adafruit INA219 begin returned false. Getter not called for this boot.");
  Serial.println("Timestamp is not a clock.");
}
void loop() {
  if (!chipOpened) {
    Serial.println("INA219 begin failed. Getter not called. Sample skipped. Timestamp is not a clock.");
    delay(3000);
    return;
  }
  cts::Sample shunt = cts::ina219ShuntMv(chip, 0);
  cts::Sample bus = cts::ina219BusV(chip, 0);
  cts::Sample current = cts::ina219CurrentMa(chip, 0);
  Serial.print("shunt=");
  Serial.print(shunt.value);
  Serial.print(" mV bus=");
  Serial.print(bus.value);
  Serial.print(" V current=");
  Serial.print(current.value);
  Serial.print(" mA");
  if (!codecReady) {
    Serial.println(". device define or begin failed. reading() not called. The line was not sent. Timestamp is not a clock.");
    delay(3000);
    return;
  }
  cts::Error shuntEncoded =
      device.reading("shunt", shunt, message, sizeof message);
  cts::Error busEncoded = device.reading("bus", bus, message, sizeof message);
  cts::Error currentEncoded =
      device.reading("current", current, message, sizeof message);
  Serial.print(" encode shunt=");
  Serial.print(cts::errorName(shuntEncoded));
  Serial.print(" bus=");
  Serial.print(cts::errorName(busEncoded));
  Serial.print(" current=");
  Serial.print(cts::errorName(currentEncoded));
  Serial.println(". Local JSON only. The line was not sent. Not a bench result. Timestamp is not a clock.");
  delay(3000);
}
