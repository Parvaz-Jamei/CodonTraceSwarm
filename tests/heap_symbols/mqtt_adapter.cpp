// SPDX-License-Identifier: MIT
// Symbol-check TU: explicit instantiation emits every ArduinoMqttAdapter member
// against a declaration-only client whose topic type is declaration-only too,
// so the client's own code appears only as undefined references.
#include "cts/ArduinoMqttAdapter.h"
struct DeclTopic {
  size_t length() const;
  const char *c_str() const;
};
struct DeclMqtt {
  bool connected();
  void onMessage(void (*)(int));
  void stop();
  void poll();
  int messageRetain() const;
  int messageQoS() const;
  DeclTopic messageTopic() const;
  int read(uint8_t *, size_t);
  int beginMessage(const char *, unsigned long, bool, uint8_t, bool);
  size_t write(const uint8_t *, size_t);
  int endMessage();
};
template class cts::ArduinoMqttAdapter<DeclMqtt, 0>;
