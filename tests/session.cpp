#include "session_fixture.h"
#include <cstring>
#include <iostream>
#include <limits>
using namespace cts;
using namespace fixture;
static int checks = 0, failures = 0;
#define CHECK(x)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(x)) {                                                                \
      ++failures;                                                              \
      std::cerr << "FAIL " << __LINE__ << " " #x "\n";                         \
    }                                                                          \
  } while (0)
struct Setup {
  Device d;
  Clock c;
  Executor e;
  CommandSession session;
  Setup() : session(d, c, e) {
    CHECK(d.define(channel()) == Error::Ok);
    CHECK(d.allow("s", Operation::Sample) == Error::Ok);
    CHECK(session.begin(config()).code == SessionCode::Ok);
  }
};
int main() {
  char out[TxBytes];
  {
    Device d;
    CHECK(!d.commandAllowed("s", Operation::Sample));
    CHECK(d.allow("s", Operation::Sample) == Error::InvalidArgument);
    auto spec = channel();
    CHECK(d.define(spec) == Error::Ok);
    CHECK(d.allow("s", static_cast<Operation>(99)) == Error::InvalidArgument);
    CHECK(d.allow(nullptr, Operation::Sample) == Error::InvalidArgument);
    CHECK(d.allow("s", Operation::SetRate) == Error::Ok);
    CHECK(d.allow("s", Operation::SetRate) == Error::Ok);
    CHECK(d.allow("s", Operation::Pipeline) == Error::Ok);
    auto b = channel("b");
    b.descriptor.type = ValueType::Boolean;
    CHECK(d.define(b) == Error::Ok);
    CHECK(d.allow("b", Operation::Pipeline) == Error::InvalidArgument);
    CHECK(d.allow("b", Operation::Capsule) == Error::InvalidArgument);
    CHECK(d.allow("b", Operation::Sample) == Error::Ok);
    spec.descriptor.maximumHz = 1000; // Must not change the owned registry.
    CHECK(d.begin(config()) == Error::Ok);
    CHECK(d.allow("s", Operation::Raw) == Error::Frozen);
    CHECK(d.commandAllowed("s", Operation::SetRate, 10));
    CHECK(!d.commandAllowed("s", Operation::SetRate, 11));
    CHECK(!d.commandAllowed("s", Operation::SetRate,
                            std::numeric_limits<double>::quiet_NaN()));
    CHECK(!d.commandAllowed("s", Operation::Sample));
    CHECK(d.commandAllowed("b", Operation::Sample));
    CHECK(!d.commandAllowed("unknown", Operation::Sample));
    CHECK(d.hello(out, sizeof(out)) == Error::Ok);
    CHECK(std::strstr(out,
                      "\"operations\":[\"sample\",\"set_rate\",\"pipeline\"]"));
    RegistryPolicy policy(d);
    Command c;
    std::strcpy(c.channel, "s");
    c.operation = Operation::SetRate;
    c.hz = 1;
    CHECK(policy.permits(c));
    c.hz = .5;
    CHECK(!policy.permits(c));
    d.end();
    CHECK(!policy.permits(c));
  }
  {
    Device d;
    CHECK(d.reply("c", true, nullptr, out, sizeof(out)) == Error::NotStarted);
    CHECK(d.define(channel()) == Error::Ok);
    CHECK(d.begin(config()) == Error::Ok);
    CHECK(d.reply("bad id", true, nullptr, out, sizeof(out)) ==
          Error::InvalidArgument);
    CHECK(d.reply("c", true, "", out, sizeof(out)) == Error::InvalidArgument);
    CHECK(d.reply("c", true, std::string(257, 'x').c_str(), out, sizeof(out)) ==
          Error::InvalidArgument);
    CHECK(d.reply("c", true, "\xc0\xaf", out, sizeof(out)) ==
          Error::InvalidArgument);
    const auto seq = d.nextSequence();
    CHECK(d.reply("c", false, "failed", out, 1) == Error::Capacity);
    CHECK(d.nextSequence() == seq);
    CHECK(d.reply("c", true, "quote\" newline\n", out, sizeof(out)) ==
          Error::Ok);
    CHECK(std::strstr(out, "\"kind\":\"reply\"") &&
          std::strstr(out, "\"ok\":true"));
    CHECK(std::strstr(out, "quote\\\" newline\\u000a"));
    CHECK(d.nextSequence() == seq + 1);
    CHECK(d.reply("c", false, nullptr, out, sizeof(out)) == Error::Ok);
    CHECK(!std::strstr(out, "message"));
  }
  {
    Setup f;
    CHECK(f.session.begin(config()).code == SessionCode::AlreadyStarted);
    CHECK(f.session.confirmReplyPublished(0) == SessionCode::NoReply);
    CHECK(f.d.hello(out, sizeof(out)) == Error::Ok);
    CHECK(submit(f.session, command()).dispatch.code == DispatchCode::Queued);
    CHECK(f.session.replySize() == 0 && f.e.calls == 0);
    CHECK(submit(f.session, command(2)).dispatch.code == DispatchCode::Queued);
    auto r = f.session.poll();
    CHECK(r.code == SessionCode::ReplyPending &&
          r.dispatch.state == CommandState::Succeeded);
    const std::string bytes = f.session.replyData();
    const auto seq = f.session.replySequence();
    CHECK(f.e.calls == 1 && f.session.pendingCommands() == 1);
    CHECK(f.session.replySize() == bytes.size());
    CHECK(std::strstr(bytes.c_str(), "\"command_id\":\"c1\""));
    for (int i = 0; i < 25; ++i) {
      CHECK(f.session.poll().code == SessionCode::ReplyPending);
      CHECK(bytes == f.session.replyData() && seq == f.session.replySequence());
      CHECK(f.e.calls == 1);
    }
    CHECK(f.d.heartbeat(out, sizeof(out)) == Error::Busy);
    CHECK(f.d.hello(out, sizeof(out)) == Error::Busy);
    CHECK(f.d.reading("s", Sample::scalar(1, T), out, sizeof(out)) ==
          Error::Busy);
    CHECK(f.d.reply("c1", true, nullptr, out, sizeof(out)) == Error::Busy);
    CHECK(f.d.nextSequence() == seq + 1);
    CHECK(f.session.confirmReplyPublished(seq + 1) ==
          SessionCode::WrongSequence);
    CHECK(f.session.replySize() == bytes.size());
    CHECK(submit(f.session, command()).dispatch.code ==
          DispatchCode::Duplicate);
    CHECK(f.session.confirmReplyPublished(seq) == SessionCode::Ok);
    CHECK(f.session.replySize() == 0);
    CHECK(f.d.heartbeat(out, sizeof(out)) == Error::Ok);
    CHECK(submit(f.session, command()).dispatch.code ==
          DispatchCode::Duplicate);
    CHECK(f.session.replySize() == 0);
    f.e.succeed = false;
    CHECK(f.session.poll().dispatch.state == CommandState::Failed);
    CHECK(std::strstr(f.session.replyData(), "\"ok\":false"));
    CHECK(f.e.calls == 2);
    CHECK(f.session.confirmReplyPublished(seq) == SessionCode::WrongSequence);
    CHECK(f.session.confirmReplyPublished(f.session.replySequence()) ==
          SessionCode::Ok);
    CHECK(f.session.poll().dispatch.code == DispatchCode::Empty);
  }
  {
    Setup f;
    CHECK(submit(f.session, command()).dispatch.code == DispatchCode::Queued);
    f.c.time.monotonicMs += 5000;
    CHECK(f.session.poll().dispatch.state == CommandState::Expired);
    CHECK(std::strstr(f.session.replyData(), "expired_before_execution"));
    CHECK(f.e.calls == 0);
  }
  {
    Setup f;
    CHECK(submit(f.session, command(1, "{\"channel\":\"s\"}", "raw"))
              .dispatch.code == DispatchCode::Denied);
    CHECK(f.session.poll().dispatch.code == DispatchCode::Empty &&
          f.session.replySize() == 0);
    CHECK(submit(f.session, command()).dispatch.code == DispatchCode::Queued);
    f.d.end();
    CHECK(f.session.poll().code == SessionCode::SessionChanged);
    CHECK(f.e.calls == 0);
    auto cfg = config();
    cfg.bootId = "different_boot";
    CHECK(f.d.begin(cfg) == Error::Ok);
    CHECK(f.session.poll().code == SessionCode::SessionChanged);
  }
  {
    Device d;
    Clock c;
    Executor e;
    CommandSession s(d, c, e);
    CHECK(s.poll().code == SessionCode::NotStarted);
    CHECK(submit(s, command()).code == SessionCode::NotStarted);
    CHECK(s.begin(config()).code == SessionCode::DeviceError);
    CHECK(d.define(channel()) == Error::Ok);
    CHECK(s.begin(config()).code == SessionCode::Ok);
    CHECK(submit(s, command()).dispatch.code ==
          DispatchCode::Denied); // default no capabilities
  }
  {
    struct Reentry : CommandExecutor {
      CommandSession *s = nullptr;
      bool protected_ = false;
      bool execute(const Command &) noexcept override {
        protected_ = s->poll().code == SessionCode::Busy &&
                     submit(*s, command(2)).code == SessionCode::Busy;
        return protected_;
      }
    } e;
    Device d;
    Clock c;
    CommandSession s(d, c, e);
    e.s = &s;
    CHECK(d.define(channel()) == Error::Ok);
    CHECK(d.allow("s", Operation::Sample) == Error::Ok);
    CHECK(s.begin(config()).code == SessionCode::Ok);
    CHECK(submit(s, command()).dispatch.code == DispatchCode::Queued);
    CHECK(s.poll().dispatch.state == CommandState::Succeeded && e.protected_);
  }
  {
    Device d;
    Clock c;
    Executor e;
    CommandSession session(d, c, e);
    CHECK(d.define(channel()) == Error::Ok);
    CHECK(d.allow("s", Operation::SetRate) == Error::Ok);
    CHECK(session.begin(config()).code == SessionCode::Ok);
    CHECK(
        submit(session, command(1, "{\"channel\":\"s\",\"hz\":11}", "set_rate"))
            .dispatch.code == DispatchCode::Denied);
    CHECK(
        submit(session, command(1, "{\"channel\":\"s\",\"hz\":2}", "set_rate"))
            .dispatch.code == DispatchCode::Queued);
    CHECK(session.poll().dispatch.state == CommandState::Succeeded &&
          e.calls == 1);
  }
  {
    Setup f;
    CHECK(submit(f.session, command()).dispatch.code == DispatchCode::Queued);
    CHECK(f.session.poll().code == SessionCode::ReplyPending);
    CHECK(submit(f.session, command(2)).dispatch.code == DispatchCode::Queued);
    f.c.time.utcMs += 5000;
    f.c.time.monotonicMs += 5000;
    CHECK(f.session.poll().code == SessionCode::ReplyPending && f.e.calls == 1);
    CHECK(f.session.confirmReplyPublished(f.session.replySequence()) ==
          SessionCode::Ok);
    CHECK(f.session.poll().dispatch.state == CommandState::Expired &&
          f.e.calls == 1);
  }
  {
    struct EndsDevice : CommandExecutor {
      Device *device = nullptr;
      bool execute(const Command &) noexcept override {
        device->end();
        return true;
      }
    } e;
    Device d;
    Clock c;
    e.device = &d;
    CommandSession session(d, c, e);
    CHECK(d.define(channel()) == Error::Ok);
    CHECK(d.allow("s", Operation::Sample) == Error::Ok);
    CHECK(session.begin(config()).code == SessionCode::Ok);
    CHECK(submit(session, command()).dispatch.code == DispatchCode::Queued);
    CHECK(session.poll().code == SessionCode::SessionChanged);
    CHECK(session.replySize() == 0);
  }
  std::cout << "session checks=" << checks << " failures=" << failures
            << " sizeof(Device)=" << sizeof(Device)
            << " sizeof(CommandSession)=" << sizeof(CommandSession) << "\n";
  return failures ? 1 : 0;
}
