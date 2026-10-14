#include <Arduino.h>
#include <Wire.h>
#include <CodonTraceSwarm.h>
#include <cts/sensors/Fdc1004Protocentral.h>

// Does not include FDCxCapSense_FDC1004.h. That header also declares FDC1004.
static cts::Device device;
static char message[cts::TxBytes];
static FDC1004 chip;
static bool chipOpened = false;
static bool codecReady = false;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  chipOpened = chip.begin();
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
  session.wifi = false;
  codecReady = device.define(channel) == cts::Error::Ok &&
               device.begin(session) == cts::Error::Ok;
  Serial.println(codecReady ? "codec ready" : "codec failed");
  Serial.println(chipOpened ? "ProtoCentral FDC1004 begin returned true. Not a bench result. Not flashed."
                            : "ProtoCentral FDC1004 begin returned false. Getter not called for this boot.");
  Serial.println("Timestamp is not a clock.");
}
void loop() {
  if (!chipOpened) {
    Serial.println("FDC1004 begin failed. Getter not called. Sample skipped. Timestamp is not a clock.");
    delay(3000);
    return;
  }
  cts::Sample sample =
      cts::protocentralFdc1004Picofarads(chip, FDC1004_CHANNEL_0, 0);
  Serial.print("capacitance=");
  Serial.print(sample.value);
  Serial.print(" pF");
  if (!codecReady) {
    Serial.println(". device define or begin failed. reading() not called. The line was not sent. Timestamp is not a clock.");
    delay(3000);
    return;
  }
  cts::Error encoded = device.reading("capacitance", sample, message, sizeof message);
  Serial.print(" valid=");
  Serial.print(sample.valid ? "true" : "false");
  Serial.print(" encode=");
  Serial.print(cts::errorName(encoded));
  Serial.println(". Local JSON only. The line was not sent. Timestamp is not a clock.");
  delay(3000);
}
