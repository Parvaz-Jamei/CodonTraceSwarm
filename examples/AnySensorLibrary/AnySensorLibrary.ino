#include <Arduino.h>
#include <Wire.h>
#include <CodonTraceSwarm.h>
#include <cts/SensorSample.h>
#include <Adafruit_BME280.h>

// A library without its own adapter. Copy the finite number into a sample.
static cts::Device device;
static char message[cts::TxBytes];
static Adafruit_BME280 chip;
static const uint64_t kNotAClockMs = 1;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  chip.begin(0x76);
  cts::ChannelSpec channel;
  channel.name = "temperature";
  channel.quantity = "temperature";
  channel.unit = "degC";
  channel.descriptor.featureId = "other-library";
  channel.descriptor.configId = "caller";
  channel.descriptor.sourceUnit = "degC";
  cts::SessionConfig session;
  session.swarmId = "sensor";
  session.nodeId = "node";
  session.bootId = "any-library";
  session.wifi = true;
  device.define(channel);
  device.begin(session);
  Serial.println("Any library: pass its number to scalarIfFinite. Not sent. Not a bench result.");
}
void loop() {
  const double fromThatLibrary = chip.readTemperature();
  cts::Sample sample = cts::scalarIfFinite(fromThatLibrary, kNotAClockMs);
  cts::Error encoded = device.reading("temperature", sample, message, sizeof message);
  Serial.print("any valid=");
  Serial.print(sample.valid ? "true" : "false");
  Serial.print(" encode=");
  Serial.println(cts::errorName(encoded));
  delay(3000);
}
