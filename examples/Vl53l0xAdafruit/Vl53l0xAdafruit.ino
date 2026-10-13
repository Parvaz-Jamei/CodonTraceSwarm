#include <Arduino.h>
#include <Wire.h>
#include <CodonTraceSwarm.h>
#include <cts/sensors/Vl53l0xAdafruit.h>

static cts::Device device;
static char message[cts::TxBytes];
static Adafruit_VL53L0X chip;
static const uint64_t kNotAClockMs = 1;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  const bool opened = chip.begin();
  cts::ChannelSpec channel;
  channel.name = "distance";
  channel.quantity = "distance";
  channel.unit = "mm";
  channel.descriptor.featureId = "vl53l0x";
  channel.descriptor.configId = "adafruit";
  channel.descriptor.sourceUnit = "mm";
  cts::SessionConfig session;
  session.swarmId = "sensor";
  session.nodeId = "node";
  session.bootId = "vl53l0x-adafruit";
  session.wifi = true;
  device.define(channel);
  device.begin(session);
  Serial.println(opened ? "Adafruit VL53L0X begin returned true. Not a bench result."
                        : "Adafruit VL53L0X begin returned false.");
}
void loop() {
  cts::Sample sample = cts::vl53l0xMillimeters(chip, kNotAClockMs);
  cts::Error encoded = device.reading("distance", sample, message, sizeof message);
  Serial.print("vl53l0x valid=");
  Serial.print(sample.valid ? "true" : "false");
  Serial.print(" encode=");
  Serial.println(cts::errorName(encoded));
  delay(3000);
}
