#include <Arduino.h>
#include <Wire.h>
#include <CodonTraceSwarm.h>
#include <cts/sensors/Fdc1004Protocentral.h>

static cts::Device device;
static char message[cts::TxBytes];
static FDC1004 chip;
static const uint64_t kNotAClockMs = 1;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  const bool opened = chip.begin();
  cts::ChannelSpec channel;
  channel.name = "capacitance";
  channel.quantity = "capacitance";
  channel.unit = "pF";
  channel.descriptor.featureId = "fdc1004";
  channel.descriptor.configId = "protocentral";
  channel.descriptor.sourceUnit = "pF";
  cts::SessionConfig session;
  session.swarmId = "sensor";
  session.nodeId = "node";
  session.bootId = "fdc1004-protocentral";
  session.wifi = true;
  device.define(channel);
  device.begin(session);
  Serial.println(opened ? "ProtoCentral FDC1004 begin returned true. Not a bench result."
                        : "ProtoCentral FDC1004 begin returned false.");
}
void loop() {
  cts::Sample sample = cts::protocentralFdc1004Picofarads(chip, FDC1004_CHANNEL_0, kNotAClockMs);
  cts::Error encoded = device.reading("capacitance", sample, message, sizeof message);
  Serial.print("fdc1004 valid=");
  Serial.print(sample.valid ? "true" : "false");
  Serial.print(" encode=");
  Serial.println(cts::errorName(encoded));
  delay(3000);
}
