// Include the harness so these checks exercise its actual limits and helpers
// without requiring libFuzzer or a machine-specific serialized fixture.
#include "fuzz_cista_archive.cpp"

#include <cstdio>
#include <exception>
#include <functional>
#include <new>

namespace {

int failures = 0;

void expect(bool condition, const char * message)
{
  if (!condition) {
    std::fprintf(stderr, "%s\n", message);
    ++failures;
  }
}

template <typename Callable> void expectReturn(const char * label, Callable&& callable)
{
  try {
    callable();
  } catch (const std::exception& error) {
    std::fprintf(stderr, "%s: unexpected exception: %s\n", label, error.what());
    ++failures;
  }
}

template <typename Error, typename Callable> void expectException(const char * label, Callable&& callable)
{
  bool caught = false;
  try {
    callable();
  } catch (const Error&) {
    caught = true;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "%s: wrong exception: %s\n", label, error.what());
  }
  expect(caught, label);
}

class CallbackLambda final : public mustache::Lambda {
  public:
    explicit CallbackLambda(std::function<std::string()> callback) :
        callback_(std::move(callback))
    {}

    std::string invoke() override
    {
      return callback_();
    }

  private:
    std::function<std::string()> callback_;
};

class UnexpectedRenderException final : public mustache::Exception {
  public:
    UnexpectedRenderException() :
        mustache::Exception("Render unexpected limit exceeded")
    {}
};

std::string repeatItems(unsigned int depth, const std::string& body)
{
  std::string source;
  for (unsigned int index = 0; index < depth; ++index) {
    source += "{{#items}}";
  }
  source += body;
  for (unsigned int index = 0; index < depth; ++index) {
    source += "{{/items}}";
  }
  return source;
}

void checkWorkload(
    const char * label, const std::string& source, const char * expectedError, const char * partialSource = nullptr)
{
  mustache::Mustache engine;
  mustache::Node root;
  engine.tokenize(source, &root);
  mustache::Node::Partials partials;
  if (partialSource != nullptr) {
    auto partial = std::make_unique<mustache::Node>();
    engine.tokenize(partialSource, partial.get());
    partials.emplace("partial", std::move(partial));
  }
  const auto archive = mustache_benchmark::serializeCistaArchive(root, partials, fuzzArchiveLimits(maxFuzzInputBytes));
  AlignedInput input(archive.data(), archive.size());
  expect(validateArchive(input.view()), "Workload must pass the harness's archive validation");

  // Establish that the renderer reaches the intended limit before checking
  // whether the harness treats that rejection as an ordinary outcome.
  bool rejected = false;
  try {
    const std::string output = mustache_benchmark::renderCistaArchive(
        input.view(), makeData(), fuzzArchiveLimits(maxFuzzInputBytes), fuzzRenderLimits());
    expect(output == "hello &lt;value&gt;", "Control must render its expected output");
  } catch (const mustache::Exception& error) {
    rejected = true;
    expect(expectedError != nullptr && std::string_view(error.what()) == expectedError,
        "Workload must hit the expected render limit");
  }
  expect(rejected == (expectedError != nullptr), "Workload render acceptance must match the expectation");

  std::printf("%s: archive validation accepted; checking both helpers\n", label);
  expectReturn("renderValidatedArchive", [&] {
    renderValidatedArchive(input.view());
  });
  expectReturn("exerciseIntegrityRepairProtocol", [&] {
    exerciseIntegrityRepairProtocol(input.data(), input.size());
  });
}

void checkLambdaExceptions()
{
  mustache::Mustache engine;
  mustache::Node root;
  engine.tokenize("{{call}}", &root);
  const auto archive = mustache_benchmark::serializeCistaArchive(root);
  AlignedInput input(archive.data(), archive.size());
  expect(validateArchive(input.view()), "Lambda archive must pass validation");

  mustache::Data data = mustache::Data::object();
  bool invoked = false;
  data.set("call", mustache::Data::lambda(std::make_unique<CallbackLambda>([&] {
    invoked = true;
    return "hello";
  })));
  expectReturn("ordinary lambda", [&] {
    renderArchive(input.view(), data);
  });
  expect(invoked, "The harness must actually render an accepted archive");

  data.set("call", mustache::Data::lambda(std::make_unique<CallbackLambda>([] {
    return std::string(16 * 1024 + 1, 'x');
  })));
  bool lambdaLimitReached = false;
  try {
    static_cast<void>(mustache_benchmark::renderCistaArchive(
        input.view(), data, fuzzArchiveLimits(maxFuzzInputBytes), fuzzRenderLimits()));
  } catch (const mustache::Exception& error) {
    lambdaLimitReached = std::string_view(error.what()) == "Render lambda template byte limit exceeded";
  }
  expect(lambdaLimitReached, "Lambda must hit the actual template-byte limit");
  expectReturn("lambda template bytes", [&] {
    renderArchive(input.view(), data);
  });

  data.set("call", mustache::Data::lambda(std::make_unique<CallbackLambda>([]() -> std::string {
    throw UnexpectedRenderException();
  })));
  expectException<UnexpectedRenderException>("Unexpected library errors must propagate unchanged", [&] {
    renderArchive(input.view(), data);
  });

  data.set("call", mustache::Data::lambda(std::make_unique<CallbackLambda>([] {
    return "{{#unclosed}}";
  })));
  expectException<mustache::TokenizerException>("Lambda parsing errors must propagate", [&] {
    renderArchive(input.view(), data);
  });

  data.set("call", mustache::Data::lambda(std::make_unique<CallbackLambda>([]() -> std::string {
    throw std::bad_alloc();
  })));
  expectException<std::bad_alloc>("Allocation errors must propagate", [&] {
    renderArchive(input.view(), data);
  });
}

} // namespace

int main()
{
  expectReturn("workload setup", [] {
    checkWorkload("control", "hello {{name}}", nullptr);
    checkWorkload("node visits", repeatItems(13, "x"), "Render node visit limit exceeded");
    checkWorkload("output bytes", repeatItems(8, std::string(300, 'x')), "Render output byte limit exceeded");
    checkWorkload("nesting", "{{>partial}}", "Render nesting limit exceeded", "{{>partial}}");
    checkWorkload("control after rejection", "hello {{name}}", nullptr);
    checkLambdaExceptions();
  });
  return failures == 0 ? 0 : 1;
}
