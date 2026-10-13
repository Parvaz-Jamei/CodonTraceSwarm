#include <Arduino.h>
#include <Wire.h>
#include <CodonTraceSwarm.h>
#include <cts/sensors/Bme280Adafruit.h>

static cts::Device device;
static char message[cts::TxBytes];
static Adafruit_BME280 chip;
static const uint64_t kNotAClockMs = 1;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  const bool opened = chip.begin(0x76);
  cts::ChannelSpec channel;
  channel.name = "temperature";
  channel.quantity = "temperature";
  channel.unit = "degC";
  channel.descriptor.featureId = "bme280";
  channel.descriptor.configId = "adafruit";
  channel.descriptor.sourceUnit = "degC";
  cts::SessionConfig session;
  session.swarmId = "sensor";
  session.nodeId = "node";
  session.bootId = "bme280-adafruit";
  session.wifi = true;
  Serial.println(device.define(channel) == cts::Error::Ok &&
                         device.begin(session) == cts::Error::Ok
                     ? "codec ready"
                     : "codec failed");
  Serial.println(opened ? "Adafruit BME280 begin returned true. Not a bench result."
                        : "Adafruit BME280 begin returned false.");
}
void loop() {
  cts::Sample sample = cts::bme280Temperature(chip, kNotAClockMs);
  cts::Error encoded = device.reading("temperature", sample, message, sizeof message);
  Serial.print("bme280 valid=");
  Serial.print(sample.valid ? "true" : "false");
  Serial.print(" encode=");
  Serial.print(cts::errorName(encoded));
  Serial.println(". Local JSON only. Not sent. Timestamp is not a clock.");
  delay(3000);
}
