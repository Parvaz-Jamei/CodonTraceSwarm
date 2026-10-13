#include <Arduino.h>
#include <CodonTraceSwarm.h>

// Builds one local hello. NOT suitable for tiny SRAM boards in this profile.
static cts::Device device;
static char message[cts::TxBytes];
static cts::Error defined = cts::Error::NotStarted;
static cts::Error started = cts::Error::NotStarted;
static cts::Error encoded = cts::Error::NotStarted;
static uint32_t lastPrint = 0;

void setup() {
  cts::ChannelSpec channel;
  channel.name = "sensor";
  channel.quantity = "temperature";
  channel.unit = "degC";
  channel.descriptor.featureId = "synthetic";
  channel.descriptor.configId = "smoke";
  channel.descriptor.sourceUnit = "degC";
  cts::SessionConfig session;
  session.swarmId = "smoke";
  session.nodeId = "node";
  session.bootId = "compile-only";
  // This sketch does not open a socket, so it is not a Wi-Fi node.
  session.wifi = false;

  Serial.begin(115200);
  defined = device.define(channel);
  started = device.begin(session);
  if (defined == cts::Error::Ok && started == cts::Error::Ok)
    encoded = device.hello(message, sizeof(message));
  else
    encoded = cts::Error::NotStarted;
}
void loop() {
  uint32_t now = static_cast<uint32_t>(millis());
  if (lastPrint != 0 && static_cast<uint32_t>(now - lastPrint) < 3000) return;
  lastPrint = now;
  Serial.print("CodonTraceSwarm core smoke define=");
  Serial.print(cts::errorName(defined));
  Serial.print(" begin=");
  Serial.print(cts::errorName(started));
  Serial.print(" hello=");
  Serial.print(cts::errorName(encoded));
  Serial.println(". Local buffer only. Not sent. No socket, no MQTT, no sensor. Timestamp is not a clock.");
  if (defined == cts::Error::Ok && started == cts::Error::Ok &&
      encoded == cts::Error::Ok)
    Serial.println(message);
}
