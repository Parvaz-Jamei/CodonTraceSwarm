#include <Arduino.h>
#include <Wire.h>
#include <CodonTraceSwarm.h>
#include <cts/sensors/FdcxCapSense.h>

// Does not include Protocentral_FDC1004.h. That header also declares FDC1004.
static cts::Device device;
static char message[cts::TxBytes];
static FDC1004 fdc1004;
static FDC2x1x fdc2214;
// Example configuration from the FDCxCapSense README, not a measured tank.
static const float kFclkHz = 40000000.0f;
static bool codecReady = false;
static bool capConfigured = false;
static bool hzStarted = false;

static bool stepOk(FDCxStatus status, const char *what) {
  if (status == FDCxStatus::Ok)
    return true;
  Serial.print(what);
  Serial.print(" failed: ");
  Serial.println(fdcxStatusToString(status));
  return false;
}

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
  session.wifi = false;
  codecReady = device.define(cap) == cts::Error::Ok &&
               device.define(hz) == cts::Error::Ok &&
               device.begin(session) == cts::Error::Ok;
  Serial.println(codecReady ? "codec ready" : "codec failed");
  Serial.print("FDCxCapSense FDC1004 begin=");
  Serial.println(opened1004 == FDCxStatus::Ok ? "ok" : "not_ok");
  Serial.print("FDC2214 begin=");
  Serial.println(opened2214 == FDCxStatus::Ok ? "ok" : "not_ok");
  Serial.println("Example constants were not measured. This sketch was not flashed. Timestamp is not a clock.");
  if (opened1004 == FDCxStatus::Ok) {
    capConfigured =
        stepOk(fdc1004.setSampleRate(FDC1004Rate::SPS_100), "setSampleRate") &&
        stepOk(fdc1004.configureSingleEnded(FDC1004Measurement::M1,
                                            FDC1004Channel::CIN1),
               "configureSingleEnded");
  } else {
    Serial.println("FDC1004 begin failed. configure not called.");
  }
  if (!capConfigured)
    Serial.println("FDC1004 not configured. Read helper not called.");
  // begin() is not a measurement start.
  if (opened2214 == FDCxStatus::Ok) {
    hzStarted =
        stepOk(fdc2214.setSleepMode(true), "setSleepMode") &&
        stepOk(fdc2214.setRCount(FDC2x1xChannel::CH0, 0x0100), "setRCount") &&
        stepOk(fdc2214.setSettleCount(FDC2x1xChannel::CH0, 0x0100),
               "setSettleCount") &&
        stepOk(fdc2214.configureClockDividers(FDC2x1xChannel::CH0, 2, 1),
               "configureClockDividers") &&
        stepOk(fdc2214.setDriveCurrentCode(FDC2x1xChannel::CH0, 15),
               "setDriveCurrentCode") &&
        stepOk(fdc2214.setDeglitch(FDC2x1xDeglitch::MHz_10), "setDeglitch") &&
        stepOk(fdc2214.startContinuous(FDC2x1xChannel::CH0), "startContinuous");
  } else {
    Serial.println("FDC2214 begin failed. configure and startContinuous not called.");
  }
  if (!hzStarted)
    Serial.println("FDC2214 not started. Frequency read helper not called.");
}

void loop() {
  if (!capConfigured) {
    Serial.println("FDC1004 begin or configure failed. Getter not called. Sample skipped.");
  } else {
    const FDCxStatus triggered =
        fdc1004.triggerSingleMeasurement(FDC1004Measurement::M1);
    if (triggered != FDCxStatus::Ok) {
      Serial.print("FDC1004 trigger failed: ");
      Serial.println(fdcxStatusToString(triggered));
      Serial.println("Read helper not called.");
    } else {
      const FDCxStatus waited =
          fdc1004.waitForMeasurement(FDC1004Measurement::M1, 100);
      if (waited != FDCxStatus::Ok) {
        Serial.print("FDC1004 wait failed: ");
        Serial.println(fdcxStatusToString(waited));
        Serial.println("Read helper not called.");
      } else {
        float picofarads = 0.0f;
        const FDCxStatus readStatus = fdc1004.readLastCapacitancePf(
            FDC1004Measurement::M1, picofarads);
        // 0 is not a clock and is not written as UTC.
        cts::Sample cap = cts::fdcxScalarFromStatus(readStatus, picofarads, 0);
        Serial.print("capacitance=");
        Serial.print(cap.value);
        Serial.print(" pF");
        if (!codecReady) {
          Serial.println(". device define or begin failed. reading() not called. The line was not sent.");
        } else {
          cts::Error encoded =
              device.reading("capacitance", cap, message, sizeof message);
          Serial.print(" valid=");
          Serial.print(cap.valid ? "true" : "false");
          Serial.print(" encode=");
          Serial.print(cts::errorName(encoded));
          Serial.println(". The line was not sent.");
        }
      }
    }
  }
  if (!hzStarted) {
    Serial.println("FDC2214 begin or start failed. Getter not called. Sample skipped.");
  } else {
    const FDCxStatus ready = fdc2214.waitDataReady(FDC2x1xChannel::CH0, 100);
    if (ready != FDCxStatus::Ok) {
      Serial.print("FDC2214 wait failed: ");
      Serial.println(fdcxStatusToString(ready));
      Serial.println("Frequency read helper not called.");
    } else {
      cts::Sample hz =
          cts::fdcxFdc2214Hertz(fdc2214, FDC2x1xChannel::CH0, kFclkHz, 0);
      Serial.print("frequency=");
      Serial.print(hz.value);
      Serial.print(" Hz");
      if (!codecReady) {
        Serial.println(". device define or begin failed. reading() not called. The line was not sent.");
      } else {
        cts::Error encoded =
            device.reading("frequency", hz, message, sizeof message);
        Serial.print(" valid=");
        Serial.print(hz.valid ? "true" : "false");
        Serial.print(" encode=");
        Serial.print(cts::errorName(encoded));
        Serial.println(". The line was not sent.");
      }
    }
  }
  Serial.println("Timestamp is not a clock. The line was not sent. Not flashed. Constants were not measured.");
  delay(3000);
}
