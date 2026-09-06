#include "mustache_config.h"

#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "mustache.hpp"

namespace {

int failures = 0;

void expect(bool condition, const char * context, const char * message)
{
  if (!condition) {
    std::fprintf(stderr, "%s: %s\n", context, message);
    ++failures;
  }
}

struct CallbackFailure {};

class CallbackLambda final : public mustache::Lambda {
  public:
    explicit CallbackLambda(std::function<void()> callback) :
        callback_(std::move(callback))
    {}

    std::string invoke() override
    {
      callback_();
      return "";
    }

  private:
    std::function<void()> callback_;
};

// Only this test is compiled with access checking disabled. Weak observers
// inspect ownership of the opaque state without changing installed headers.
// External owners stay alive throughout every render, including failing checks.
template <typename Handle, typename Render> void testRootLifetime(const char * context, Handle handle, Render render)
{
  const std::weak_ptr<const void> observed = handle.state;
  expect(observed.use_count() == 1, context, "fixture has an unexpected extra owner");

  for (bool throwFromCallback : {false, true}) {
    int calls = 0;
    auto data = mustache::Data::object();
    data.set("probe", mustache::Data::lambda(std::make_unique<CallbackLambda>([&]() {
      ++calls;
      expect(observed.use_count() > 1, context, "active render did not retain its root");
      if (throwFromCallback) {
        throw CallbackFailure{};
      }
    })));

    bool propagated = false;
    try {
      expect(render(handle, data) == "beforeafter", context, "rendered output changed");
    } catch (const CallbackFailure&) {
      propagated = true;
    }
    expect(calls == 1, context, "root callback did not run exactly once");
    expect(propagated == throwFromCallback, context, "callback exception propagation changed");
    expect(observed.use_count() == 1, context, "completed render retained its root");
  }

  handle = Handle();
  expect(observed.expired(), context, "root state outlived its last owner after rendering");
}

void testNestedPartialLifetime()
{
  constexpr const char * context = "nested compiled partials";
  const auto root = mustache::compile("{{>outer}}{{done}}");
  mustache::PartialMap partials;
  partials.emplace("outer", mustache::compile("O{{probe}}{{>inner}}{{after}}"));
  partials.emplace("inner", mustache::compile("I{{probe}}"));
  const std::weak_ptr<const void> outer = partials.at("outer").state;
  const std::weak_ptr<const void> inner = partials.at("inner").state;
  expect(outer.use_count() == 1 && inner.use_count() == 1, context, "fixture has an unexpected extra owner");

  for (bool throwFromCallback : {false, true}) {
    int probes = 0;
    int after = 0;
    int done = 0;
    auto data = mustache::Data::object();
    data.set("probe", mustache::Data::lambda(std::make_unique<CallbackLambda>([&]() {
      ++probes;
      expect(outer.use_count() > 1, context, "active outer partial was not retained");
      if (probes == 1) {
        expect(inner.use_count() == 1, context, "unused inner partial was retained");
      } else {
        expect(inner.use_count() > 1, context, "active inner partial was not retained");
        if (throwFromCallback) {
          throw CallbackFailure{};
        }
      }
    })));
    data.set("after", mustache::Data::lambda(std::make_unique<CallbackLambda>([&]() {
      ++after;
      expect(outer.use_count() > 1, context, "outer partial was released before its render finished");
      expect(inner.use_count() == 1, context, "completed inner partial was retained");
    })));
    data.set("done", mustache::Data::lambda(std::make_unique<CallbackLambda>([&]() {
      ++done;
      expect(outer.use_count() == 1 && inner.use_count() == 1, context, "completed partials were retained");
    })));

    bool propagated = false;
    try {
      expect(mustache::render(root, data, partials) == "OI", context, "rendered output changed");
    } catch (const CallbackFailure&) {
      propagated = true;
    }
    expect(probes == 2, context, "partial callbacks did not run exactly twice");
    expect(after == (throwFromCallback ? 0 : 1), context, "inner completion callback count changed");
    expect(done == (throwFromCallback ? 0 : 1), context, "outer completion callback count changed");
    expect(propagated == throwFromCallback, context, "callback exception propagation changed");
    expect(outer.use_count() == 1 && inner.use_count() == 1, context, "render retained partials after completion");
  }

  partials.clear();
  expect(outer.expired() && inner.expired(), context, "partial state outlived its last owner after rendering");
}

} // namespace

int main()
{
  const auto renderFree = [](const auto& handle, const mustache::Data& data) {
    return mustache::render(handle, data);
  };
  const mustache::Mustache engine;
  const auto renderMember = [&](const auto& handle, const mustache::Data& data) {
    return engine.render(handle, data, mustache::RenderLimits());
  };
  testRootLifetime("compiled root/free", mustache::compile("before{{probe}}after"), renderFree);
  testRootLifetime("compiled root/member", mustache::compile("before{{probe}}after"), renderMember);
  testNestedPartialLifetime();
#if defined(MUSTACHE_HAVE_ARCHIVED_TEMPLATES)
  const auto bytes = mustache::serializeArchivedTemplate(mustache::compile("before{{probe}}after"));
  testRootLifetime("archived root/free", mustache::loadArchivedTemplate(bytes), renderFree);
  testRootLifetime("archived root/member", mustache::loadArchivedTemplate(bytes), renderMember);
#endif
  return failures == 0 ? 0 : 1;
}
