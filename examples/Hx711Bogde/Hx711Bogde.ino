#include <Arduino.h>
#include <CodonTraceSwarm.h>
#include <cts/sensors/Hx711Bogde.h>

static cts::Device device;
static char message[cts::TxBytes];
static HX711 chip;
static const uint64_t kNotAClockMs = 1;

void setup() {
  Serial.begin(115200);
  chip.begin(2, 3);
  cts::ChannelSpec channel;
  channel.name = "mass";
  channel.quantity = "mass";
  channel.unit = "scale";
  channel.descriptor.featureId = "hx711";
  channel.descriptor.configId = "bogde";
  channel.descriptor.sourceUnit = "scale";
  cts::SessionConfig session;
  session.swarmId = "sensor";
  session.nodeId = "node";
  session.bootId = "hx711-bogde";
  session.wifi = true;
  device.define(channel);
  device.begin(session);
  Serial.println("HX711 Arduino Library begin called. Scale is the library default. Not a calibration.");
}
void loop() {
  cts::Sample sample = cts::hx711Units(chip, kNotAClockMs);
  cts::Error encoded = device.reading("mass", sample, message, sizeof message);
  Serial.print("hx711 valid=");
  Serial.print(sample.valid ? "true" : "false");
  Serial.print(" encode=");
  Serial.println(cts::errorName(encoded));
  delay(3000);
}
