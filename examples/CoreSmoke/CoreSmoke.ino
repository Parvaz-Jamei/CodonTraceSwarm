#include <Arduino.h>
#include <CodonTraceSwarm.h>
// Full core smoke sketch, NOT suitable for tiny SRAM boards in this profile.
static cts::Device device;
static char message[cts::TxBytes];
volatile bool coreReady = false;
void setup() {
  cts::ChannelSpec channel;
  channel.name="sensor"; channel.quantity="temperature"; channel.unit="degC";
  channel.descriptor.featureId="synthetic";
  channel.descriptor.configId="smoke";
  channel.descriptor.sourceUnit="degC";
  cts::SessionConfig session;
  session.swarmId="smoke"; session.nodeId="node"; session.bootId="compile-only";
  // wifi=true is only the genesis_swarm_v2 eligibility flag. This sketch
  // opens no socket, no MQTT, and no sensor. It is a compile-oriented
  // smoke, not a link test and not a measurement.
  session.wifi=true;
  coreReady = device.define(channel)==cts::Error::Ok &&
      device.begin(session)==cts::Error::Ok &&
      device.hello(message,sizeof(message))==cts::Error::Ok;
}
void loop() {}
