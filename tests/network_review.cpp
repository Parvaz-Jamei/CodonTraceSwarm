// SPDX-License-Identifier: MIT
// Independent adversarial network-ingress tests; no live broker.
#include "cts/NetworkInbox.h"
#include "session_fixture.h"
#include <iostream>
#include <cstring>
#include <limits>
using namespace cts;
int checks=0,failures=0;
#define CHECK(x) do{++checks;if(!(x)){++failures;std::cerr<<__LINE__<<": "<<#x<<"\n";}}while(0)
struct F {
 Device d;fixture::Clock c;fixture::Executor e;CommandSession session;NetworkInbox inbox;
 F():session(d,c,e){CHECK(d.define(fixture::channel())==Error::Ok);CHECK(d.allow("s",Operation::Sample)==Error::Ok);CHECK(session.begin(fixture::config()).code==SessionCode::Ok);CHECK(inbox.bind("lab","node")==InboxCode::Ok);CHECK(inbox.connected(1)==InboxCode::Ok);}
 ReceiveFragment fragment(const std::string &s,uint64_t token=1){ReceiveFragment r;r.topic=inbox.topic();r.topicLength=std::strlen(r.topic);r.data=s.data();r.length=r.total=s.size();r.connection=1;r.token=token;r.authenticated=r.retainKnown=true;r.retained=false;return r;}
};
int main(){
 {F f;auto w=fixture::command();auto r=f.fragment(w);r.retainKnown=false;CHECK(f.inbox.push(r)==InboxCode::UnknownRetain);r.retainKnown=true;r.authenticated=false;CHECK(f.inbox.push(r)==InboxCode::Untrusted);r.authenticated=true;r.retained=true;CHECK(f.inbox.push(r)==InboxCode::Retained);r.retained=false;r.topic="wrong";r.topicLength=5;CHECK(f.inbox.push(r)==InboxCode::WrongTopic);CHECK(f.e.calls==0);CHECK(f.inbox.size()==0);}
 {F f;auto w=fixture::command();auto r=f.fragment(w);r.length=20;CHECK(f.inbox.push(r)==InboxCode::Partial);CHECK(f.inbox.size()==0);r.data=w.data()+20;r.length=w.size()-20;r.offset=20;r.topic=nullptr;r.topicLength=0;CHECK(f.inbox.push(r)==InboxCode::Enqueued);std::fill(w.begin(),w.end(),'x');CHECK(f.e.calls==0);CHECK(f.inbox.pollOne(f.session).code==InboxCode::Ok);CHECK(f.e.calls==0);CHECK(f.session.poll().code==SessionCode::ReplyPending);CHECK(f.e.calls==1);CHECK(f.inbox.size()==0);}
 {F f;auto w=fixture::command();auto r=f.fragment(w);r.length=10;CHECK(f.inbox.push(r)==InboxCode::Partial);r.offset=11;r.data=w.data()+11;r.length=5;CHECK(f.inbox.push(r)==InboxCode::InvalidFragment);r.offset=10;r.data=w.data()+10;CHECK(f.inbox.push(r)==InboxCode::InvalidFragment);CHECK(f.inbox.health().abandoned==1);}
 {F f;auto w=fixture::command();auto r=f.fragment(w);r.length=10;CHECK(f.inbox.push(r)==InboxCode::Partial);auto stale=r;stale.connection=999;CHECK(f.inbox.push(stale)==InboxCode::WrongConnection);r.offset=10;r.data=w.data()+10;r.length=w.size()-10;CHECK(f.inbox.push(r)==InboxCode::Enqueued);CHECK(f.inbox.connected(2)==InboxCode::Ok);CHECK(f.inbox.size()==0);CHECK(f.inbox.health().disconnectDrops==1);CHECK(f.inbox.push(r)==InboxCode::WrongConnection);CHECK(f.inbox.connected(2)==InboxCode::WrongConnection);}
 {F f;auto w=fixture::command();auto r=f.fragment(w);r.offset=std::numeric_limits<size_t>::max();r.length=10;CHECK(f.inbox.push(r)==InboxCode::InvalidFragment);r=f.fragment(w);r.total=CommandBytes+1;CHECK(f.inbox.push(r)==InboxCode::TooLarge);r=f.fragment(w);r.token=0;CHECK(f.inbox.push(r)==InboxCode::InvalidFragment);}
 {F f;for(int i=1;i<=3;++i){auto w=fixture::command(i);CHECK(f.inbox.push(f.fragment(w,i))==InboxCode::Enqueued);}auto w=fixture::command(4);CHECK(f.inbox.push(f.fragment(w,4))==InboxCode::Full);f.c.time.synchronized=false;CHECK(f.inbox.pollOne(f.session).code==InboxCode::Retrying);CHECK(f.inbox.size()==3);f.c.time.synchronized=true;CHECK(f.inbox.pollOne(f.session).code==InboxCode::Ok);CHECK(f.inbox.size()==2);CHECK(f.session.poll().code==SessionCode::ReplyPending);CHECK(f.inbox.pollOne(f.session).code==InboxCode::Ok);CHECK(f.session.poll().code==SessionCode::ReplyPending);CHECK(f.e.calls==1);CHECK(f.session.confirmReplyPublished(f.session.replySequence())==SessionCode::Ok);CHECK(f.session.poll().code==SessionCode::ReplyPending);CHECK(f.e.calls==2);}
 {F f;NetworkInbox wrong;CHECK(wrong.bind("other","other")==InboxCode::Ok);CHECK(wrong.connected(1)==InboxCode::Ok);auto w=fixture::command();auto r=f.fragment(w);r.topic=wrong.topic();r.topicLength=std::strlen(r.topic);CHECK(wrong.push(r)==InboxCode::Enqueued);CHECK(wrong.pollOne(f.session).code==InboxCode::ScopeMismatch);CHECK(f.session.pendingCommands()==0);CHECK(wrong.size()==1);CHECK(f.e.calls==0);}
 std::cout<<"independent_network_checks="<<checks<<" failures="<<failures<<"\n";return failures?1:0;
}
