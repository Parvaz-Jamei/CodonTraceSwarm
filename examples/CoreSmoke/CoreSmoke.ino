// Prints a hello and a reading. Does not open a socket.
#include <Arduino.h>
#include <CodonTraceSwarm.h>

static cts::Device device;
static char helloJson[cts::TxBytes];
static char readingJson[cts::TxBytes];
static cts::Error defined = cts::Error::NotStarted;
static cts::Error started = cts::Error::NotStarted;
static cts::Error encoded = cts::Error::NotStarted;

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
  session.wifi = true;

  Serial.begin(115200);
  defined = device.define(channel);
  if (defined == cts::Error::Ok)
    started = device.begin(session);
  if (defined == cts::Error::Ok && started == cts::Error::Ok)
    encoded = device.hello(helloJson, sizeof(helloJson));
}

void loop() {
  if (defined != cts::Error::Ok) {
    Serial.println(cts::errorName(defined));
  } else if (started != cts::Error::Ok) {
    Serial.println(cts::errorName(started));
  } else if (encoded != cts::Error::Ok) {
    Serial.println(cts::errorName(encoded));
  } else {
    const cts::Sample sample = cts::Sample::scalar(20.0, 1800000000000ULL);
    const cts::Error reading =
        device.reading("sensor", sample, readingJson, sizeof(readingJson));
    if (reading != cts::Error::Ok) {
      Serial.println(cts::errorName(reading));
    } else {
      Serial.println(helloJson);
      Serial.println(readingJson);
    }
  }
  delay(3000);
}
