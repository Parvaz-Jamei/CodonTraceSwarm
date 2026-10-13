#include <Arduino.h>
#include <CodonTraceSwarm.h>

// Builds one local hello. NOT suitable for tiny SRAM boards in this profile.
static cts::Device device;
static char message[cts::TxBytes];

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
  // wifi=true is only the genesis_swarm_v2 eligibility flag.
  session.wifi = true;

  Serial.begin(115200);
  for (int i = 0; i < 50 && !Serial; ++i) delay(10);
  cts::Error defined = device.define(channel);
  cts::Error started = device.begin(session);
  cts::Error encoded = device.hello(message, sizeof(message));
  Serial.print("CodonTraceSwarm core smoke define=");
  Serial.print(cts::errorName(defined));
  Serial.print(" begin=");
  Serial.print(cts::errorName(started));
  Serial.print(" hello=");
  Serial.print(cts::errorName(encoded));
  Serial.println(". Local buffer only. Not sent. No socket, no MQTT, no sensor.");
  if (defined == cts::Error::Ok && started == cts::Error::Ok &&
      encoded == cts::Error::Ok)
    Serial.println(message);
}
void loop() {}
