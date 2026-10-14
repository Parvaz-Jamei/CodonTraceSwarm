// Prints the reading on Serial.
#include <Arduino.h>
#include <Wire.h>
#include <CodonTraceSwarm.h>
#include <cts/sensors/FdcxCapSense.h>

static FDC1004 fdc1004;
static FDC2x1x fdc2214;
static const float kFclkHz = 40000000.0f;
static bool capOpened = false;
static bool capConfigured = false;
static const char *capSetupStatus = "read_failed";
static bool hzOpened = false;
static bool hzStarted = false;
static const char *hzSetupStatus = "read_failed";

static bool remember(FDCxStatus status, const char *&why) {
  if (status == FDCxStatus::Ok)
    return true;
  why = fdcxStatusToString(status);
  return false;
}

static void printRaw(bool &first, const char *text) {
  if (!first)
    Serial.print(" | ");
  first = false;
  Serial.print(text);
}

static void printField(bool &first, const char *name, const cts::Sample &sample,
                       const char *unit) {
  if (!first)
    Serial.print(" | ");
  first = false;
  Serial.print(name);
  Serial.print('=');
  if (sample.valid) {
    Serial.print(sample.value);
    Serial.print(' ');
    Serial.print(unit);
  } else {
    Serial.print(sample.status ? sample.status : "");
  }
}

static void printNamedStatus(bool &first, const char *name, const char *status) {
  if (!first)
    Serial.print(" | ");
  first = false;
  Serial.print(name);
  Serial.print('=');
  Serial.print(status ? status : "");
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  const FDCxStatus opened1004 = fdc1004.begin(Wire);
  const FDCxStatus opened2214 = fdc2214.begin(Wire, FDC2x1xVariant::FDC2214);
  capOpened = opened1004 == FDCxStatus::Ok;
  hzOpened = opened2214 == FDCxStatus::Ok;
  if (capOpened) {
    capConfigured =
        remember(fdc1004.setSampleRate(FDC1004Rate::SPS_100), capSetupStatus) &&
        remember(fdc1004.configureSingleEnded(FDC1004Measurement::M1,
                                              FDC1004Channel::CIN1),
                 capSetupStatus);
  }
  if (hzOpened) {
    hzStarted =
        remember(fdc2214.setSleepMode(true), hzSetupStatus) &&
        remember(fdc2214.setRCount(FDC2x1xChannel::CH0, 0x0100), hzSetupStatus) &&
        remember(fdc2214.setSettleCount(FDC2x1xChannel::CH0, 0x0100),
                 hzSetupStatus) &&
        remember(fdc2214.configureClockDividers(FDC2x1xChannel::CH0, 2, 1),
                 hzSetupStatus) &&
        remember(fdc2214.setDriveCurrentCode(FDC2x1xChannel::CH0, 15),
                 hzSetupStatus) &&
        remember(fdc2214.setDeglitch(FDC2x1xDeglitch::MHz_10), hzSetupStatus) &&
        remember(fdc2214.startContinuous(FDC2x1xChannel::CH0), hzSetupStatus);
  }
}

void loop() {
  bool first = true;
  if (!capOpened) {
    printRaw(first, "FDC1004 begin_failed");
  } else if (!capConfigured) {
    printNamedStatus(first, "capacitance", capSetupStatus);
  } else {
    const FDCxStatus triggered =
        fdc1004.triggerSingleMeasurement(FDC1004Measurement::M1);
    if (triggered != FDCxStatus::Ok) {
      printNamedStatus(first, "capacitance", fdcxStatusToString(triggered));
    } else {
      const FDCxStatus waited =
          fdc1004.waitForMeasurement(FDC1004Measurement::M1, 100);
      if (waited != FDCxStatus::Ok) {
        printNamedStatus(first, "capacitance", fdcxStatusToString(waited));
      } else {
        float picofarads = 0.0f;
        const FDCxStatus readStatus = fdc1004.readLastCapacitancePf(
            FDC1004Measurement::M1, picofarads);
        const cts::Sample cap =
            cts::fdcxScalarFromStatus(readStatus, picofarads, 0);
        printField(first, "capacitance", cap, "pF");
      }
    }
  }
  if (!hzOpened) {
    printRaw(first, "FDC2214 begin_failed");
  } else if (!hzStarted) {
    printNamedStatus(first, "frequency", hzSetupStatus);
  } else {
    const FDCxStatus ready = fdc2214.waitDataReady(FDC2x1xChannel::CH0, 100);
    if (ready != FDCxStatus::Ok) {
      printNamedStatus(first, "frequency", fdcxStatusToString(ready));
    } else {
      const cts::Sample hz =
          cts::fdcxFdc2214Hertz(fdc2214, FDC2x1xChannel::CH0, kFclkHz, 0);
      printField(first, "frequency", hz, "Hz");
    }
  }
  Serial.println();
  delay(3000);
}
