#include <Arduino.h>
#include <Wire.h>
#include <CodonTraceSwarm.h>
#include <cts/sensors/Vl53l0xAdafruit.h>

static cts::Device device;
static char message[cts::TxBytes];
static Adafruit_VL53L0X chip;
static bool chipOpened = false;
static bool codecReady = false;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  chipOpened = chip.begin();
  cts::ChannelSpec channel;
  channel.name = "distance";
  channel.quantity = "distance";
  channel.unit = "mm";
  channel.descriptor.featureId = "vl53l0x";
  channel.descriptor.configId = "adafruit";
  channel.descriptor.sourceUnit = "mm";
  cts::SessionConfig session;
  session.swarmId = "sensor";
  session.nodeId = "node";
  session.bootId = "vl53l0x-adafruit";
  session.wifi = false;
  codecReady = device.define(channel) == cts::Error::Ok &&
               device.begin(session) == cts::Error::Ok;
  Serial.println(codecReady ? "codec ready" : "codec failed");
  Serial.println(chipOpened ? "Adafruit VL53L0X begin returned true. Not a bench result. Not flashed."
                            : "Adafruit VL53L0X begin returned false. Getter not called for this boot.");
  Serial.println("Timestamp is not a clock.");
}
void loop() {
  if (!chipOpened) {
    Serial.println("VL53L0X begin failed. Getter not called. Sample skipped. Timestamp is not a clock.");
    delay(3000);
    return;
  }
  cts::Sample sample = cts::vl53l0xMillimeters(chip, 0);
  Serial.print("distance=");
  Serial.print(sample.value);
  Serial.print(" mm");
  if (!codecReady) {
    Serial.println(". device define or begin failed. reading() not called. The line was not sent. Timestamp is not a clock.");
    delay(3000);
    return;
  }
  cts::Error encoded = device.reading("distance", sample, message, sizeof message);
  Serial.print(" valid=");
  Serial.print(sample.valid ? "true" : "false");
  Serial.print(" encode=");
  Serial.print(cts::errorName(encoded));
  Serial.println(". Local JSON only. The line was not sent. Not a bench result. Timestamp is not a clock.");
  delay(3000);
}
