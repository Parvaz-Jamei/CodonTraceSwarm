#include "cts/ReconnectPolicy.h"
#include "cts/NetworkInbox.h"
#include <cassert>
#include <iostream>
int main() {
  using namespace cts;
  ReconnectConfig c;c.initialMs=10;c.maximumMs=40;c.timeoutMs=5;c.stableMs=20;
  ReconnectPolicy p(c); NetworkInbox inbox;
  assert(inbox.bind("lab","node")==InboxCode::Ok);
  assert(p.start(0));assert(p.poll(0,false,0).action==LinkAction::None);
  auto first=p.poll(0,true,0);assert(first.action==LinkAction::Connect);
  assert(p.connected(first.token,1,true,0));
  assert(inbox.connected(first.token)==InboxCode::Ok);
  ReceiveFragment f;f.topic="genesis/swarm/lab/node/down";f.topicLength=27;
  f.data="{";f.length=1;f.total=2;f.connection=first.token;f.token=1;
  f.authenticated=true;f.retainKnown=true;f.retained=false;
  assert(inbox.push(f)==InboxCode::Partial);
  // Simulated transport loss: adapter hands the same event to policy and inbox.
  assert(p.lost(first.token,2,0));assert(inbox.disconnected()==InboxCode::Ok);
  assert(inbox.health().abandoned==1);
  assert(p.nextAttemptMs()==7);
  assert(p.poll(6,true,0).action==LinkAction::None);
  auto next=p.poll(7,true,0);assert(next.action==LinkAction::Connect);
  assert(next.token>first.token);assert(!p.connected(first.token,8,true,0));
  assert(p.connected(next.token,8,true,0));assert(inbox.connected(next.token)==InboxCode::Ok);
  assert(inbox.push(f)==InboxCode::WrongConnection);
  auto close=p.poll(9,false,0);assert(close.action==LinkAction::Close);
  assert(inbox.disconnected()==InboxCode::Ok);
  assert(p.state()==LinkState::Waiting);
  auto due=p.nextAttemptMs();assert(p.poll(due,true,0).action==LinkAction::Connect);
  assert(p.poll(due+5,true,0).action==LinkAction::Close);
  assert(p.health().timeouts==1);
  assert(p.poll(due+4,true,0).action==LinkAction::Close);
  assert(p.state()==LinkState::Fault);assert(!p.start(due+6));
  std::cout<<"PASS: reconnect policy -> actual NetworkInbox connection generations, partial-drop, stale input and clock gating (synthetic transport events)\n";
}
