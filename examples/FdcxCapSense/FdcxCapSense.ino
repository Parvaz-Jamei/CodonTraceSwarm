#include <Arduino.h>
#include <Wire.h>
#include <CodonTraceSwarm.h>
#include <cts/sensors/FdcxCapSense.h>

static cts::Device device;
static char message[cts::TxBytes];
static FDC1004 fdc1004;
static FDC2x1x fdc2214;
static const uint64_t kNotAClockMs = 1;
// Not measured on this sketch. Pass the board's real reference clock.
static const float kFclkHz = 40000000.0f;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  const FDCxStatus opened1004 = fdc1004.begin(Wire);
  const FDCxStatus opened2214 = fdc2214.begin(Wire, FDC2x1xVariant::FDC2214);
  cts::ChannelSpec cap;
  cap.name = "capacitance";
  cap.quantity = "capacitance";
  cap.unit = "pF";
  cap.descriptor.featureId = "fdc1004";
  cap.descriptor.configId = "fdcx";
  cap.descriptor.sourceUnit = "pF";
  cts::ChannelSpec hz;
  hz.name = "frequency";
  hz.quantity = "frequency";
  hz.unit = "Hz";
  hz.descriptor.featureId = "fdc2214";
  hz.descriptor.configId = "fdcx";
  hz.descriptor.sourceUnit = "Hz";
  cts::SessionConfig session;
  session.swarmId = "sensor";
  session.nodeId = "node";
  session.bootId = "fdcx";
  session.wifi = true;
  device.define(cap);
  device.define(hz);
  device.begin(session);
  Serial.print("FDCxCapSense FDC1004 begin=");
  Serial.print(opened1004 == FDCxStatus::Ok ? "ok" : "not_ok");
  Serial.print(" FDC2214 begin=");
  Serial.println(opened2214 == FDCxStatus::Ok ? "ok" : "not_ok");
  Serial.println("Not a bench result. FDC2214 clock is the caller constant. That clock was not measured.");
}
void loop() {
  cts::Sample cap = cts::fdcxFdc1004Picofarads(fdc1004, FDC1004Measurement::M1, kNotAClockMs);
  cts::Sample hz = cts::fdcxFdc2214Hertz(fdc2214, FDC2x1xChannel::CH0, kFclkHz, kNotAClockMs);
  cts::Error capEncoded = device.reading("capacitance", cap, message, sizeof message);
  cts::Error hzEncoded = device.reading("frequency", hz, message, sizeof message);
  Serial.print("fdc1004 valid=");
  Serial.print(cap.valid ? "true" : "false");
  Serial.print(" encode=");
  Serial.print(cts::errorName(capEncoded));
  Serial.print(" fdc2214 valid=");
  Serial.print(hz.valid ? "true" : "false");
  Serial.print(" encode=");
  Serial.println(cts::errorName(hzEncoded));
  delay(3000);
}
