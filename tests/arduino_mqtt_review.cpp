// SPDX-License-Identifier: MIT
// Independent test: real upstream MqttClient.cpp, fake byte transport/Arduino facade.
#include <MqttClient.h>
#include "cts/ArduinoMqttAdapter.h"
#include "session_fixture.h"
#include <deque>
#include <vector>
#include <iostream>
#include <algorithm>
unsigned long testArduinoMillis = 0;
void (*testArduinoYieldHook)() = nullptr;
static unsigned checks=0, failures=0;
#define CHECK(x) do { ++checks; if(!(x)) { ++failures; std::cerr<<__LINE__<<": "<<#x<<'\n'; } } while(false)
using Bytes=std::vector<uint8_t>;
static Bytes frame(const std::string& data, uint8_t flags=2, const std::string& topic="genesis/swarm/lab/node/down", size_t announced=0) {
  Bytes b{static_cast<uint8_t>(0x30|flags)};
  size_t n=(announced?announced:data.size())+topic.size()+2+((flags&6)?2:0);
  do { uint8_t v=n%128;n/=128;b.push_back(v|(n?128:0)); } while(n);
  b.push_back(topic.size()>>8);b.push_back(topic.size()&255);
  b.insert(b.end(),topic.begin(),topic.end());
  if(flags&6) { b.push_back(0);b.push_back(42); }
  b.insert(b.end(),data.begin(),data.end());return b;
}
class Wire final:public Client {
public:
  std::deque<uint8_t> rx;
  Bytes tx,assembling,injectBeforeAck;
  bool alive=false, ack=true, shortWrite=false, wrongAck=false;
  unsigned stops=0,publishes=0,writes=0,shortAt=0;
  void feed(const Bytes& b) { rx.insert(rx.end(),b.begin(),b.end()); }
  int connect(IPAddress,uint16_t) override { alive=true;feed({0x20,2,0,0});return 1; }
  int connect(const char*,uint16_t p) override { return connect(IPAddress(),p); }
  size_t write(uint8_t b) override { return write(&b,1); }
  size_t write(const uint8_t* b,size_t n) override {
    ++writes;
    const size_t count=(shortWrite || writes==shortAt) && n?n-1:n;
    tx.insert(tx.end(),b,b+count);assembling.insert(assembling.end(),b,b+count);
    while(assembling.size()>=2) {
      size_t remain=0,m=1,i=1;
      do { if(i>=assembling.size()) return count;remain+=(assembling[i]&127)*m;m*=128; } while(assembling[i++]&128);
      if(assembling.size()<i+remain) break;
      if((assembling[0]>>4)==3) {
        ++publishes;const size_t topic=(assembling[i]<<8)|assembling[i+1];
        const size_t at=i+2+topic;
        feed(injectBeforeAck);injectBeforeAck.clear();
        if(ack && (assembling[0]&6)) feed({0x40,2,assembling[at],static_cast<uint8_t>(assembling[at+1]+(wrongAck?1:0))});
      }
      assembling.erase(assembling.begin(),assembling.begin()+i+remain);
    }
    return count;
  }
  int available() override { return static_cast<int>(rx.size()); }
  int read() override { if(rx.empty()) return -1;auto v=rx.front();rx.pop_front();return v; }
  int read(uint8_t* b,size_t n) override { size_t i=0;while(i<n&&!rx.empty()) b[i++]=static_cast<uint8_t>(read());return static_cast<int>(i); }
  int peek() override { return rx.empty()?-1:rx.front(); }
  void flush() override {}
  void stop() override { alive=false;++stops; }
  uint8_t connected() override { return alive; }
  operator bool() override { return true; }
};
static Wire* delayedWire=nullptr;
static Bytes delayedBytes;
static void deliverDelayed() {
  testArduinoYieldHook=nullptr;delayedWire->feed(delayedBytes);delayedBytes.clear();
}
struct Rig {
  Wire wire;MqttClient mqtt;cts::NetworkInbox inbox;cts::ArduinoMqttAdapter<MqttClient> adapter;
  cts::Device device;fixture::Clock clock;fixture::Executor exec;cts::CommandSession session;
  Rig():mqtt(wire),adapter(mqtt,inbox),session(device,clock,exec) {
    mqtt.setId("native-review");mqtt.setConnectionTimeout(20);mqtt.setKeepAliveInterval(1000000);
    CHECK(mqtt.connect("synthetic-byte-stream",1883)==1);
    CHECK(inbox.bind("lab","node")==cts::InboxCode::Ok);
    CHECK(device.define(fixture::channel())==cts::Error::Ok);
    CHECK(device.allow("s",cts::Operation::Sample)==cts::Error::Ok);
    CHECK(session.begin(fixture::config()).code==cts::SessionCode::Ok);
    CHECK(adapter.attach(1,true));wire.tx.clear();
  }
  void reply() {
    CHECK(fixture::submit(session,fixture::command()).dispatch.code==cts::DispatchCode::Queued);
    CHECK(session.poll().code==cts::SessionCode::ReplyPending);
  }
  cts::ArduinoMqttCode drain(uint64_t now=100) {
    return adapter.receive(now);
  }
};
int main() {
  using cts::ArduinoMqttCode;
  {
    Rig r;const auto packet=frame(fixture::command());
    // Incomplete fixed header then remaining bytes: retain parser state across polls.
    const size_t cut=1;
    r.wire.feed(Bytes(packet.begin(),packet.begin()+cut));
    CHECK(r.adapter.receive(100)==ArduinoMqttCode::Idle);
    CHECK(r.exec.calls==0 && r.inbox.size()==0);
    r.wire.feed(Bytes(packet.begin()+cut,packet.end()));
    CHECK(r.drain(101)==ArduinoMqttCode::Enqueued);
    CHECK(r.exec.calls==0 && r.inbox.size()==1);
    CHECK(r.inbox.pollOne(r.session).code==cts::InboxCode::Ok);
    CHECK(r.session.poll().code==cts::SessionCode::ReplyPending);
    CHECK(r.exec.calls==1);
    const std::string reply(r.session.replyData(),r.session.replySize());
    CHECK(r.adapter.publishReply(r.session)==ArduinoMqttCode::Published);
    CHECK(r.session.replySize()==0 && r.wire.publishes==1);
    CHECK(std::search(r.wire.tx.begin(),r.wire.tx.end(),reply.begin(),reply.end())!=r.wire.tx.end());
  }
  for(unsigned variant=0;variant<5;++variant) {
    Rig r;
    if(variant==0) r.wire.feed(frame("x",3)); // retained
    if(variant==1) r.wire.feed(frame("x",0)); // qos0
    if(variant==2) r.wire.feed(frame("x",2,"genesis/swarm/lab/other/down"));
    if(variant==3) r.wire.feed(frame("x",2,"genesis/swarm/lab/node/down",cts::CommandBytes+1));
    if(variant==4) r.wire.feed(frame("")); // Empty commands fail closed too.
    CHECK(r.adapter.receive(100)==ArduinoMqttCode::Rejected);
    CHECK(!r.wire.alive && r.inbox.size()==0 && r.exec.calls==0);
  }
  {
    Rig r;const Bytes b=frame(fixture::command());const size_t cut=b.size()-fixture::command().size()+64;
    r.wire.feed(Bytes(b.begin(),b.begin()+cut));
    delayedWire=&r.wire;delayedBytes=Bytes(b.begin()+cut,b.end());testArduinoYieldHook=&deliverDelayed;
    CHECK(r.adapter.receive(100)==ArduinoMqttCode::Enqueued);
    CHECK(testArduinoYieldHook==nullptr && delayedBytes.empty());
    CHECK(r.inbox.size()==1 && r.exec.calls==0);
  }
  {
    Rig r;r.wire.feed(frame("short",2,"genesis/swarm/lab/node/down",200));
    CHECK(r.adapter.receive(100)==ArduinoMqttCode::Rejected);
    CHECK(!r.wire.alive && r.inbox.size()==0);
  }
  for(unsigned variant=0;variant<4;++variant) {
    Rig r;r.reply();const std::string before(r.session.replyData(),r.session.replySize());
    if(variant==0) r.wire.ack=false;
    if(variant==1) r.wire.shortWrite=true;
    if(variant==2) r.wire.wrongAck=true;
    if(variant==3) r.wire.shortAt=r.wire.writes+2; // Payload, not just header.
    CHECK(r.adapter.publishReply(r.session)==ArduinoMqttCode::Rejected);
    CHECK(!r.wire.alive);
    CHECK(std::string(r.session.replyData(),r.session.replySize())==before);
  }
  {
    Rig r;r.reply();
    r.wire.injectBeforeAck=frame(fixture::command(2));
    CHECK(r.adapter.publishReply(r.session)==ArduinoMqttCode::Published);
    CHECK(r.inbox.size()==1); // Regression: poll-only adapter previously discarded this command.
    CHECK(r.exec.calls==1);
    CHECK(r.inbox.pollOne(r.session).code==cts::InboxCode::Ok);
    CHECK(r.session.poll().code==cts::SessionCode::ReplyPending);
    CHECK(r.exec.calls==2);
  }
  {
    Rig r;
    cts::Device other;fixture::Clock c;fixture::Executor e;cts::CommandSession s(other,c,e);
    CHECK(other.define(fixture::channel())==cts::Error::Ok);
    CHECK(other.allow("s",cts::Operation::Sample)==cts::Error::Ok);
    auto cfg=fixture::config();cfg.nodeId="other";
    CHECK(s.begin(cfg).code==cts::SessionCode::Ok);
    CHECK(r.adapter.publishReply(s)==ArduinoMqttCode::Rejected);
    CHECK(!r.wire.alive);
  }
  {
    Rig r;Wire wire;MqttClient mqtt(wire);cts::NetworkInbox inbox;
    mqtt.setId("second");mqtt.setConnectionTimeout(20);
    CHECK(mqtt.connect("fake",1883)==1);
    CHECK(inbox.bind("lab","other")==cts::InboxCode::Ok);
    {
      cts::ArduinoMqttAdapter<MqttClient> conflict(mqtt,inbox);
      CHECK(!conflict.attach(1,true));
      CHECK(r.wire.alive);
    }
    r.wire.feed(frame(fixture::command()));
    CHECK(r.adapter.receive(100)==ArduinoMqttCode::Enqueued);
    CHECK(r.inbox.size()==1); // Rejected second owner did not unregister first.
    r.adapter.close();CHECK(r.inbox.size()==0);
    {
      cts::ArduinoMqttAdapter<MqttClient> replacement(mqtt,inbox);
      CHECK(replacement.attach(1,true));
    }
    CHECK(!wire.alive && wire.stops>0); // Owner destructor closes its transport.
  }
  {
    Rig r;r.adapter.close();
    CHECK(r.mqtt.connect("fake",1883)==1);
    CHECK(!r.adapter.attach(2,false));CHECK(!r.wire.alive);
    CHECK(r.mqtt.connect("fake",1883)==1);
    CHECK(r.adapter.attach(2,true));
    CHECK(r.adapter.receive(100)==ArduinoMqttCode::Idle);
    CHECK(r.adapter.receive(99)==ArduinoMqttCode::Rejected);
    CHECK(!r.wire.alive);
  }
  std::cout<<"Independent real ArduinoMqttClient adapter review: "<<checks<<" checks, "<<failures<<" failures\n";
  return failures?1:0;
}
