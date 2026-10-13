#include <Arduino.h>
#include <CodonTraceSwarm.h>
#include <cts/sensors/Hx711Bogde.h>

static cts::Device device;
static char message[cts::TxBytes];
static HX711 chip;
// HX711::begin returns void and cannot report a chip. This stays false only
// when begin was not completed.
static bool chipBegun = false;
static bool codecReady = false;

void setup() {
  Serial.begin(115200);
  chip.begin(2, 3);
  chipBegun = true;
  cts::ChannelSpec channel;
  channel.name = "library_scale";
  channel.quantity = "library_scale";
  channel.unit = "scale";
  channel.descriptor.featureId = "hx711";
  channel.descriptor.configId = "bogde";
  channel.descriptor.sourceUnit = "scale";
  cts::SessionConfig session;
  session.swarmId = "sensor";
  session.nodeId = "node";
  session.bootId = "hx711-bogde";
  session.wifi = false;
  codecReady = device.define(channel) == cts::Error::Ok &&
               device.begin(session) == cts::Error::Ok;
  Serial.println(codecReady ? "codec ready" : "codec failed");
  Serial.println("HX711 Arduino Library begin returns void and does not prove the chip answered. The quantity is library_scale, unit scale, not a physical mass. Not a calibration. Not flashed. Timestamp is not a clock.");
}
void loop() {
  if (!chipBegun) {
    Serial.println("HX711 begin was not completed. Getter not called. Sample skipped. Timestamp is not a clock.");
    delay(3000);
    return;
  }
  cts::Sample sample = cts::hx711Units(chip, 0);
  if (!codecReady) {
    Serial.println("device define or begin failed. reading() not called. Sample skipped. Timestamp is not a clock.");
    delay(3000);
    return;
  }
  cts::Error encoded =
      device.reading("library_scale", sample, message, sizeof message);
  Serial.print("hx711 valid=");
  Serial.print(sample.valid ? "true" : "false");
  Serial.print(" encode=");
  Serial.print(cts::errorName(encoded));
  Serial.println(". Local JSON only. Not sent. Timestamp is not a clock.");
  delay(3000);
}
