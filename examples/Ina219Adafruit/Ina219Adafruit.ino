#include <Arduino.h>
#include <Wire.h>
#include <CodonTraceSwarm.h>
#include <cts/sensors/Ina219Adafruit.h>

static cts::Device device;
static char message[cts::TxBytes];
static Adafruit_INA219 chip;
static const uint64_t kNotAClockMs = 1;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  const bool opened = chip.begin();
  cts::ChannelSpec channel;
  channel.name = "current";
  channel.quantity = "current";
  channel.unit = "mA";
  channel.descriptor.featureId = "ina219";
  channel.descriptor.configId = "adafruit";
  channel.descriptor.sourceUnit = "mA";
  cts::SessionConfig session;
  session.swarmId = "sensor";
  session.nodeId = "node";
  session.bootId = "ina219-adafruit";
  session.wifi = true;
  device.define(channel);
  device.begin(session);
  Serial.println(opened ? "Adafruit INA219 begin returned true. Not a bench result."
                        : "Adafruit INA219 begin returned false.");
}
void loop() {
  cts::Sample sample = cts::ina219CurrentMa(chip, kNotAClockMs);
  cts::Error encoded = device.reading("current", sample, message, sizeof message);
  Serial.print("ina219 valid=");
  Serial.print(sample.valid ? "true" : "false");
  Serial.print(" encode=");
  Serial.println(cts::errorName(encoded));
  delay(3000);
}
