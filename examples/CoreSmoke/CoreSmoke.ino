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
  session.wifi=false; // No transport is created by this example.
  coreReady = device.define(channel)==cts::Error::Ok &&
      device.begin(session)==cts::Error::Ok &&
      device.hello(message,sizeof(message))==cts::Error::Ok;
}
void loop() {}
