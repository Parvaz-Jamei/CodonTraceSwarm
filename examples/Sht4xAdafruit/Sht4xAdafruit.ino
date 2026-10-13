#include <Arduino.h>
#include <Wire.h>
#include <CodonTraceSwarm.h>
#include <cts/sensors/Sht4xAdafruit.h>

static cts::Device device;
static char message[cts::TxBytes];
static Adafruit_SHT4x chip;
static const uint64_t kNotAClockMs = 1;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  const bool opened = chip.begin();
  cts::ChannelSpec channel;
  channel.name = "temperature";
  channel.quantity = "temperature";
  channel.unit = "degC";
  channel.descriptor.featureId = "sht4x";
  channel.descriptor.configId = "adafruit";
  channel.descriptor.sourceUnit = "degC";
  cts::SessionConfig session;
  session.swarmId = "sensor";
  session.nodeId = "node";
  session.bootId = "sht4x-adafruit";
  session.wifi = true;
  device.define(channel);
  device.begin(session);
  Serial.println(opened ? "Adafruit SHT4x begin returned true. Not a bench result."
                        : "Adafruit SHT4x begin returned false.");
}
void loop() {
  cts::Sample sample = cts::sht4xTemperature(chip, kNotAClockMs);
  cts::Error encoded = device.reading("temperature", sample, message, sizeof message);
  Serial.print("sht4x valid=");
  Serial.print(sample.valid ? "true" : "false");
  Serial.print(" encode=");
  Serial.println(cts::errorName(encoded));
  delay(3000);
}
