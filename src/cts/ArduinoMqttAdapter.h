// SPDX-License-Identifier: MIT
#pragma once
#include "NetworkInbox.h"
#include <cstring>
namespace cts {
enum class ArduinoMqttCode { Idle, Enqueued, Closed, Rejected, Busy, Published, NoReply };
// Optional synchronous ArduinoMqttClient 0.1.8 callback adapter, no SDK includes.
// One live adapter per <Mqtt,Slot>; use a distinct Slot for another client.
// Exclusive owner: no external onMessage registration, parseMessage/read/poll.
// Mqtt and inbox outlive adapter. Single-threaded; SDK may block and allocate.
template<class Mqtt, unsigned Slot=0> class ArduinoMqttAdapter {
public:
  ArduinoMqttAdapter(Mqtt &mqtt, NetworkInbox &inbox) : mqtt_(mqtt), inbox_(inbox) {}
  ~ArduinoMqttAdapter() { if (owner_==this) close(); }
  ArduinoMqttAdapter(const ArduinoMqttAdapter &) = delete;
  ArduinoMqttAdapter &operator=(const ArduinoMqttAdapter &) = delete;
  // Caller attests verified CA/hostname, valid TLS clock, authenticated MQTT and subscription.
  // This class cannot determine those from generic Client::connected().
  bool attach(uint64_t generation, bool verifiedTlsAndClockAndAuth) {
    if (online_ || (owner_ && owner_!=this)) return false;
    if (!verifiedTlsAndClockAndAuth || !mqtt_.connected()) { close(); return false; }
    const char *topic=inbox_.topic();const size_t n=std::strlen(topic);
    if (n<20 || n>=sizeof(up_) || std::strncmp(topic,"genesis/swarm/",14) ||
        std::strcmp(topic+n-5,"/down")) { close();return false; }
    const char *split=std::strchr(topic+14,'/');
    if (!split || split<=topic+14 || split-topic-14>63 ||
        topic+n-5<=split+1 || topic+n-5-(split+1)>63) { close();return false; }
    const size_t sn=static_cast<size_t>(split-topic-14), nn=static_cast<size_t>(topic+n-5-(split+1));
    std::memcpy(swarm_,topic+14,sn);swarm_[sn]=0;
    std::memcpy(node_,split+1,nn);node_[nn]=0;
    if(inbox_.connected(generation)!=InboxCode::Ok) { close();return false; }
    std::memcpy(up_,topic,n-4);std::memcpy(up_+n-4,"up",3);
    generation_=generation;online_=true;seen_=false;owner_=this;
    mqtt_.onMessage(&dispatch);return true;
  }
  void close() {
    if(owner_==this) { mqtt_.onMessage(nullptr);owner_=nullptr; }
    mqtt_.stop();inbox_.disconnected();online_=false;
  }
  ArduinoMqttCode receive(uint64_t now) {
    if(inCallback_) return ArduinoMqttCode::Busy;
    if(!online_) return ArduinoMqttCode::Closed;
    if(!mqtt_.connected() || (seen_ && now<last_)) return fail();
    seen_=true;last_=now;event_=ArduinoMqttCode::Idle;
    mqtt_.poll(); // The callback queues bytes, including during PUBACK waits.
    if(!online_ || !mqtt_.connected()) { close();return ArduinoMqttCode::Rejected; }
    return event_;
  }
  ArduinoMqttCode publishReply(CommandSession &session) {
    if(inCallback_) return ArduinoMqttCode::Busy;
    if(!online_ || !mqtt_.connected()) { close();return ArduinoMqttCode::Closed; }
    if(!session.matchesRoute(swarm_,node_)) return fail();
    if(!session.replySize()) return ArduinoMqttCode::NoReply;
    const auto seq=session.replySequence();const size_t n=session.replySize();
    if(!mqtt_.beginMessage(up_,static_cast<unsigned long>(n),false,1,false) ||
       mqtt_.write(reinterpret_cast<const uint8_t*>(session.replyData()),n)!=n ||
       !mqtt_.endMessage() || !online_) return fail();
    if(session.confirmReplyPublished(seq)!=SessionCode::Ok) return fail();
    return ArduinoMqttCode::Published;
  }
private:
  static void dispatch(int length) { if(owner_) owner_->message(length); }
  void message(int length) {
    if(inCallback_ || !online_) { fail();return; }
    inCallback_=true;
    if(length<=0 || static_cast<size_t>(length)>CommandBytes ||
       mqtt_.messageRetain()!=0 || mqtt_.messageQoS()!=1 || token_==UINT64_MAX) {
      fail();inCallback_=false;return;
    }
    const auto topic=mqtt_.messageTopic();
    if(topic.length()!=std::strlen(inbox_.topic()) || std::strcmp(topic.c_str(),inbox_.topic())) {
      fail();inCallback_=false;return;
    }
    ++token_;const size_t total=static_cast<size_t>(length);
    for(size_t offset=0;offset<total;) {
      const size_t count=total-offset>sizeof(chunk_)?sizeof(chunk_):total-offset;
      const int got=mqtt_.read(reinterpret_cast<uint8_t*>(chunk_),count);
      if(got<=0 || static_cast<size_t>(got)!=count) { fail();break; }
      ReceiveFragment f;f.topic=inbox_.topic();f.topicLength=std::strlen(f.topic);
      f.data=chunk_;f.length=count;f.total=total;f.offset=offset;
      f.connection=generation_;f.token=token_;f.authenticated=true;
      f.retainKnown=true;f.retained=false;
      const auto result=inbox_.push(f);
      if(result!=InboxCode::Partial && result!=InboxCode::Enqueued) { fail();break; }
      offset+=count;
      if(offset==total) event_=ArduinoMqttCode::Enqueued;
    }
    inCallback_=false;
  }
  ArduinoMqttCode fail() { event_=ArduinoMqttCode::Rejected;close();return event_; }
  static ArduinoMqttAdapter *owner_;
  Mqtt &mqtt_;NetworkInbox &inbox_;
  char up_[160]={},swarm_[64]={},node_[64]={},chunk_[128]={};
  uint64_t generation_=0,token_=0,last_=0;
  bool online_=false,inCallback_=false,seen_=false;
  ArduinoMqttCode event_=ArduinoMqttCode::Idle;
};
template<class Mqtt,unsigned Slot> ArduinoMqttAdapter<Mqtt,Slot>* ArduinoMqttAdapter<Mqtt,Slot>::owner_=nullptr;
}
