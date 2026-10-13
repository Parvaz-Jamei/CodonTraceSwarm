#include "cts/NetworkInbox.h"
#include "session_fixture.h"
#include <cstring>
#include <iostream>
using namespace cts;
static int checks = 0, failures = 0;
#define CHECK(x)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(x)) {                                                                \
      ++failures;                                                              \
      std::cerr << "FAIL " << __LINE__ << " " #x "\n";                         \
    }                                                                          \
  } while (0)
ReceiveFragment fragment(const std::string &s, size_t offset = 0,
                         size_t count = 0) {
  ReceiveFragment f;
  f.topic = "genesis/swarm/lab/node/down";
  f.topicLength = std::strlen(f.topic);
  f.data = s.data() + offset;
  f.length = count ? count : s.size() - offset;
  f.total = s.size();
  f.offset = offset;
  f.connection = 1;
  f.token = 1;
  f.authenticated = true;
  f.retainKnown = true;
  f.retained = false;
  return f;
}
struct Setup {
  Device d;
  fixture::Clock c;
  fixture::Executor e;
  CommandSession session;
  NetworkInbox inbox;
  Setup() : session(d, c, e) {
    CHECK(d.define(fixture::channel()) == Error::Ok);
    CHECK(d.allow("s", Operation::Sample) == Error::Ok);
    CHECK(session.begin(fixture::config()).code == SessionCode::Ok);
    CHECK(inbox.bind("lab", "node") == InboxCode::Ok);
    CHECK(inbox.connected(1) == InboxCode::Ok);
  }
};
int main() {
  auto wire = fixture::command();
  {
    NetworkInbox i;
    CHECK(i.push(fragment(wire)) == InboxCode::NotBound);
    CHECK(i.bind(nullptr, "node") == InboxCode::InvalidScope);
    CHECK(i.bind("lab", "node") == InboxCode::Ok);
    CHECK(i.bind("lab", "node") == InboxCode::AlreadyBound);
    CHECK(i.push(fragment(wire)) == InboxCode::Disconnected);
    CHECK(i.connected(0) == InboxCode::WrongConnection);
    CHECK(i.connected(1) == InboxCode::Ok);
    CHECK(i.connected(1) == InboxCode::WrongConnection);
  }
  for (int bad = 0; bad < 8; ++bad) {
    Setup s;
    auto f = fragment(wire);
    const InboxCode expected[] = {
        InboxCode::WrongConnection, InboxCode::Untrusted,
        InboxCode::UnknownRetain,   InboxCode::Retained,
        InboxCode::WrongTopic,      InboxCode::InvalidFragment,
        InboxCode::TooLarge,        InboxCode::InvalidFragment};
    switch (bad) {
    case 0:
      f.connection = 2;
      break;
    case 1:
      f.authenticated = false;
      break;
    case 2:
      f.retainKnown = false;
      break;
    case 3:
      f.retained = true;
      break;
    case 4:
      f.topicLength--;
      break;
    case 5:
      f.token = 0;
      break;
    case 6:
      f.total = CommandBytes + 1;
      break;
    case 7:
      f.offset = f.total + 1;
      break;
    }
    CHECK(s.inbox.push(f) == expected[bad]);
    CHECK(s.inbox.size() == 0 && s.e.calls == 0);
    CHECK(s.inbox.health().rejected == 1);
  }
  {
    Setup s;
    std::string owned = wire;
    CHECK(s.inbox.push(fragment(owned)) == InboxCode::Enqueued);
    owned.assign("destroyed");
    CHECK(s.e.calls == 0);
    CHECK(s.inbox.pollOne(s.session).code == InboxCode::Ok);
    CHECK(s.e.calls == 0);
    CHECK(s.session.poll().dispatch.state == CommandState::Succeeded);
    CHECK(s.e.calls == 1);
  }
  for (size_t cut = 1; cut < wire.size(); ++cut) {
    Setup s;
    auto f = fragment(wire, 0, cut);
    CHECK(s.inbox.push(f) == InboxCode::Partial);
    CHECK(s.inbox.pollOne(s.session).code == InboxCode::Empty);
    f = fragment(wire, cut);
    f.topic = nullptr;
    f.topicLength = 0;
    CHECK(s.inbox.push(f) == InboxCode::Enqueued);
    CHECK(s.inbox.pollOne(s.session).code == InboxCode::Ok);
    CHECK(s.session.poll().dispatch.state == CommandState::Succeeded &&
          s.e.calls == 1);
  }
  {
    Setup s;
    CHECK(s.inbox.push(fragment(wire, 0, 10)) == InboxCode::Partial);
    CHECK(s.inbox.push(fragment(wire, 11)) == InboxCode::InvalidFragment);
    CHECK(s.inbox.health().abandoned == 1);
    CHECK(s.inbox.push(fragment(wire, 10)) == InboxCode::InvalidFragment);
  }
  {
    Setup s;
    for (int j = 0; j < 3; ++j)
      CHECK(s.inbox.push(fragment(fixture::command(j + 1))) ==
            InboxCode::Enqueued);
    CHECK(s.inbox.push(fragment(wire)) == InboxCode::Full);
    CHECK(s.inbox.size() == 3 && s.inbox.health().full == 1);
    s.c.time.synchronized = false;
    CHECK(s.inbox.pollOne(s.session).code == InboxCode::Retrying);
    CHECK(s.inbox.size() == 3);
    s.c.time.synchronized = true;
    for (int j = 0; j < 3; ++j)
      CHECK(s.inbox.pollOne(s.session).code == InboxCode::Ok);
    CHECK(s.inbox.size() == 0 && s.e.calls == 0);
    CHECK(s.inbox.health().drained == 3);
  }
  {
    Setup s;
    CHECK(s.inbox.push(fragment(wire)) == InboxCode::Enqueued);
    CHECK(s.inbox.push(fragment(wire, 0, 10)) == InboxCode::Partial);
    CHECK(s.inbox.disconnected() == InboxCode::Ok);
    CHECK(s.inbox.size() == 0 && s.inbox.health().disconnectDrops == 2);
    CHECK(s.inbox.connected(1) == InboxCode::WrongConnection);
    CHECK(s.inbox.connected(2) == InboxCode::Ok);
    CHECK(s.inbox.push(fragment(wire)) == InboxCode::WrongConnection);
    auto f = fragment(wire);
    f.connection = 2;
    CHECK(s.inbox.push(f) == InboxCode::Enqueued);
  }
  {
    Setup s;
    std::string full = wire + std::string(CommandBytes - wire.size(), ' ');
    CHECK(s.inbox.push(fragment(full)) == InboxCode::Enqueued);
    CHECK(s.inbox.pollOne(s.session).code == InboxCode::Ok);
  }
  {
    Setup s;
    CHECK(s.inbox.push(fragment("{}")) == InboxCode::Enqueued);
    auto r = s.inbox.pollOne(s.session);
    CHECK(r.code == InboxCode::Rejected &&
          r.session.dispatch.code == DispatchCode::DecodeRejected);
    CHECK(s.inbox.size() == 0 && s.e.calls == 0);
  }
  std::cout << "network_checks=" << checks << " failures=" << failures
            << " sizeof(NetworkInbox)=" << sizeof(NetworkInbox) << "\n";
  return failures ? 1 : 0;
}
