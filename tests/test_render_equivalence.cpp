#include "mustache_config.h"

#if !defined(MUSTACHE_HAVE_ARCHIVED_TEMPLATES)
#error "test_render_equivalence requires archived-template support"
#endif

#include "archived_template.hpp"
#include "data.hpp"
#include "exception.hpp"
#include "lambda.hpp"
#include "mustache.hpp"
#include "node.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <limits>
#include <map>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t casesPerSeed = 64;
constexpr std::size_t generatedFragmentKinds = 13;

class Random {
  public:
    explicit Random(std::uint64_t seed) :
        engine_(seed)
    {}

    std::size_t index(std::size_t bound)
    {
      return static_cast<std::size_t>(engine_() % bound);
    }

  private:
    std::mt19937_64 engine_;
};

struct Coverage {
    std::array<std::size_t, generatedFragmentKinds> fragments{};
    std::size_t arrayContexts = 0;
    std::size_t listContexts = 0;
    std::size_t emptyLists = 0;
    std::size_t nonemptyLists = 0;
    std::size_t embeddedNuls = 0;
    std::size_t variableLambdas = 0;
    std::size_t sectionLambdas = 0;
    std::size_t rootOwnedPartials = 0;
    std::size_t externalPartialOverrides = 0;
    std::size_t exactOutputLimits = 0;
    std::size_t rejectedOutputLimits = 0;
    std::size_t exactNodeVisitLimits = 0;
    std::size_t rejectedNodeVisitLimits = 0;
};

struct GeneratedCase {
    std::string source;
    std::map<std::string, std::string> partialSources;
    mustache::Data data;
};

class TemplateLambda final : public mustache::Lambda {
  public:
    TemplateLambda(std::string response, std::size_t * calls) :
        response_(std::move(response)),
        calls_(calls)
    {}

    std::string invoke() override
    {
      ++*calls_;
      return response_;
    }

    std::string invoke(std::string_view, mustache::LambdaRenderContext) override
    {
      ++*calls_;
      return response_;
    }

  private:
    std::string response_;
    std::size_t * calls_;
};

std::string escaped(std::string_view value)
{
  static constexpr char hex[] = "0123456789abcdef";
  std::string result;
  result.reserve(value.size());
  for (const unsigned char byte : value) {
    switch (byte) {
      case '\\':
        result.append("\\\\");
        break;
      case '\n':
        result.append("\\n");
        break;
      case '\r':
        result.append("\\r");
        break;
      case '\t':
        result.append("\\t");
        break;
      default:
        if (byte < 0x20 || byte >= 0x7f) {
          result.append("\\x");
          result.push_back(hex[byte >> 4]);
          result.push_back(hex[byte & 0x0f]);
        } else {
          result.push_back(static_cast<char>(byte));
        }
        break;
    }
  }
  return result;
}

std::string randomValue(Random& random, std::size_t caseIndex, Coverage& coverage)
{
  static constexpr std::string_view alphabet = "abc XYZ09<&>\"'\n\r\t";
  const std::size_t length = random.index(17);
  std::string value;
  value.reserve(length + 1);
  for (std::size_t index = 0; index < length; ++index) {
    value.push_back(alphabet[random.index(alphabet.size())]);
  }
  if (caseIndex % 17 == 0) {
    value.insert(value.size() / 2, 1, '\0');
    ++coverage.embeddedNuls;
  }
  return value;
}

std::string randomLiteral(Random& random, std::size_t caseIndex)
{
  static constexpr std::string_view alphabet = "literal ABC xyz 019-_.:/\n\r\t";
  const std::size_t length = random.index(19);
  std::string value;
  value.reserve(length + 1);
  for (std::size_t index = 0; index < length; ++index) {
    value.push_back(alphabet[random.index(alphabet.size())]);
  }
  if (caseIndex % 23 == 0) {
    value.insert(value.size() / 2, 1, '\0');
  }
  return value;
}

mustache::Data makeData(Random& random, std::size_t caseIndex, Coverage& coverage)
{
  mustache::Data data = mustache::Data::object();
  data.set("title", mustache::Data::string(randomValue(random, caseIndex, coverage)));

  static constexpr std::array<std::int64_t, 7> integers = {std::numeric_limits<std::int64_t>::min(), -1, 0, 1, 42,
      9007199254740991LL, std::numeric_limits<std::int64_t>::max()};
  data.set("count", mustache::Data::integer(integers[caseIndex % integers.size()]));

  static constexpr std::array<double, 8> floatingValues = {
      0.0, -0.0, 1.1, 1e-4, -2.5, 1e100, std::numeric_limits<double>::min(), std::numeric_limits<double>::max()};
  data.set("ratio", mustache::Data::floating(floatingValues[caseIndex % floatingValues.size()]));
  data.set("enabled", mustache::Data::boolean(caseIndex % 2 != 0));
  data.set("missingLike", mustache::Data::null());
  data.set("variableLambda",
      mustache::Data::lambda(std::make_unique<TemplateLambda>("{{user.name}}", &coverage.variableLambdas)));
  data.set("sectionLambda",
      mustache::Data::lambda(std::make_unique<TemplateLambda>("[{{title}}/{{>footer}}]", &coverage.sectionLambdas)));

  mustache::Data address = mustache::Data::object();
  address.set("city", mustache::Data::string(randomValue(random, caseIndex + 1, coverage)));
  mustache::Data user = mustache::Data::object();
  user.set("name", mustache::Data::string(randomValue(random, caseIndex + 2, coverage)));
  user.set("html", mustache::Data::string(randomValue(random, caseIndex + 3, coverage)));
  user.set("active", mustache::Data::boolean(caseIndex % 3 != 0));
  user.set("address", std::move(address));
  data.set("user", std::move(user));

  const std::size_t itemCount = caseIndex % 5;
  if (itemCount == 0) {
    ++coverage.emptyLists;
  } else {
    ++coverage.nonemptyLists;
  }
  mustache::Data items = caseIndex % 4 < 2 ? mustache::Data::array() : mustache::Data::list();
  if (items.type() == mustache::Data::TypeArray) {
    ++coverage.arrayContexts;
  } else {
    ++coverage.listContexts;
  }
  for (std::size_t itemIndex = 0; itemIndex < itemCount; ++itemIndex) {
    mustache::Data item = mustache::Data::object();
    item.set("name", mustache::Data::string(randomValue(random, caseIndex + itemIndex + 4, coverage)));
    item.set("value", mustache::Data::integer(static_cast<std::int64_t>(random.index(2001)) - 1000));
    item.set("visible", mustache::Data::boolean((caseIndex + itemIndex) % 2 != 0));
    mustache::Data tags = mustache::Data::array();
    const std::size_t tagCount = (caseIndex + itemIndex) % 4;
    for (std::size_t tagIndex = 0; tagIndex < tagCount; ++tagIndex) {
      tags.push_back(mustache::Data::string(randomValue(random, caseIndex + tagIndex + 8, coverage)));
    }
    item.set("tags", std::move(tags));
    items.push_back(std::move(item));
  }
  data.set("items", std::move(items));

  switch (caseIndex % 5) {
    case 0:
      data.set("scalar", mustache::Data::null());
      break;
    case 1:
      data.set("scalar", mustache::Data::boolean(true));
      break;
    case 2:
      data.set("scalar", mustache::Data::integer(-17));
      break;
    case 3:
      data.set("scalar", mustache::Data::floating(1.0000000000000002));
      break;
    default:
      data.set("scalar", mustache::Data::string(randomValue(random, caseIndex + 12, coverage)));
      break;
  }
  return data;
}

void appendGeneratedFragment(
    std::string * source, Random& random, std::size_t caseIndex, std::size_t fragmentIndex, Coverage& coverage)
{
  const std::size_t kind = (caseIndex + fragmentIndex + random.index(generatedFragmentKinds)) % generatedFragmentKinds;
  ++coverage.fragments[kind];
  switch (kind) {
    case 0:
      source->append("{{title}}");
      break;
    case 1:
      source->append("{{{title}}}");
      break;
    case 2:
      source->append("{{&user.html}}");
      break;
    case 3:
      source->append("{{#enabled}}yes{{/enabled}}");
      break;
    case 4:
      source->append("{{^enabled}}no{{/enabled}}");
      break;
    case 5:
      source->append("{{#user}}{{#active}}{{address.city}}{{/active}}{{/user}}");
      break;
    case 6:
      source->append("{{#items}}{{name}}/{{#tags}}{{.}},{{/tags}}{{/items}}");
      break;
    case 7:
      source->append("({{>footer}})");
      break;
    case 8:
      source->append("{{! ignored generated comment }}");
      break;
    case 9:
      source->append("{{missing.value}}");
      break;
    case 10:
      source->append(randomLiteral(random, caseIndex));
      break;
    case 11:
      source->append("\r\n\t{{>card}}\r\n");
      break;
    case 12:
      source->append("{{=<% %>=}}<%scalar%><%={{ }}=%>");
      break;
    default:
      throw std::logic_error("unreachable generated fragment kind");
  }
}

GeneratedCase makeCase(Random& random, std::size_t caseIndex, Coverage& coverage)
{
  GeneratedCase generated{"{{! structured property case }}\n"
                          "{{title}}|{{{title}}}|{{&title}}|{{count}}|{{ratio}}|{{missingLike}}\n"
                          "{{#enabled}}enabled{{/enabled}}{{^enabled}}disabled{{/enabled}}\n"
                          "{{#user}}{{name}}@{{address.city}}/{{{html}}}{{/user}}\n"
                          "{{#items}}{{name}}={{value}}:{{#visible}}V{{/visible}}{{^visible}}H{{/visible}}:"
                          "{{#tags}}[{{.}}]{{/tags}}:{{>row}}{{/items}}\n"
                          "{{variableLambda}}|{{#sectionLambda}}raw {{title}}{{/sectionLambda}}\n"
                          "  {{>card}}\n"
                          "{{=<% %>=}}<%title%><%={{ }}=%>\n",
      {{"card", "CARD {{user.name}}/{{title}}\n {{>footer}}\n"}, {"footer", "F={{count}}/{{scalar}}\n"},
          {"row", "[{{name}}={{value}}{{#tags}}<{{.}}>{{/tags}}]\n"}},
      makeData(random, caseIndex, coverage)};

  const std::size_t fragmentCount = 4 + random.index(9);
  for (std::size_t fragmentIndex = 0; fragmentIndex < fragmentCount; ++fragmentIndex) {
    appendGeneratedFragment(&generated.source, random, caseIndex, fragmentIndex, coverage);
    generated.source.push_back('|');
  }
  return generated;
}

mustache::Node::Partials tokenizePartials(mustache::Mustache& engine, const std::map<std::string, std::string>& sources)
{
  mustache::Node::Partials partials;
  for (const auto& entry : sources) {
    auto partial = std::make_unique<mustache::Node>();
    engine.tokenize(entry.second, partial.get());
    partials.emplace(entry.first, std::move(partial));
  }
  return partials;
}

std::string_view byteView(const std::vector<std::uint8_t>& bytes)
{
  return std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size());
}

mustache::Node::Partials legacyRoundTripPartials(const mustache::Node::Partials& source)
{
  mustache::Node::Partials result;
  for (const auto& entry : source) {
    if (entry.second == nullptr) {
      result.emplace(entry.first, std::unique_ptr<mustache::Node>());
      continue;
    }
    const std::vector<std::uint8_t> bytes = entry.second->serializeValue();
    std::unique_ptr<mustache::Node> decoded = mustache::Node::unserializeOwned(byteView(bytes));
    if (decoded->serializeValue() != bytes) {
      throw std::runtime_error("legacy partial serialization was not byte-stable");
    }
    result.emplace(entry.first, std::move(decoded));
  }
  return result;
}

[[noreturn]] void reportMismatch(const char * representation, const std::string& expected, const std::string& actual)
{
  std::fprintf(stderr,
      "render equivalence failed: representation=%s\n"
      "  expected: %s\n"
      "  actual:   %s\n",
      representation, escaped(expected).c_str(), escaped(actual).c_str());
  throw std::runtime_error("generated representations rendered differently");
}

template <typename Render>
void expectRenderRejected(
    const char * representation, const char * expectedMessage, Render&& render, const mustache::RenderLimits& limits)
{
  try {
    static_cast<void>(render(limits));
  } catch (const mustache::Exception& exception) {
    if (std::string_view(exception.what()) == expectedMessage) {
      return;
    }
    throw std::runtime_error(std::string(representation) + " rejected with an unexpected error: " + exception.what());
  }
  throw std::runtime_error(std::string(representation) + " unexpectedly accepted a constrained render");
}

void checkCase(const GeneratedCase& generated, Coverage& coverage)
{
  mustache::Mustache engine;
  mustache::Node root;
  engine.tokenize(generated.source, &root);
  mustache::Node::Partials partials = tokenizePartials(engine, generated.partialSources);

  const std::vector<std::uint8_t> legacyBytes = root.serializeValue();
  std::unique_ptr<mustache::Node> legacyRoot = mustache::Node::unserializeOwned(byteView(legacyBytes));
  if (legacyRoot->serializeValue() != legacyBytes) {
    throw std::runtime_error("legacy root serialization was not byte-stable");
  }
  mustache::Node::Partials legacyPartials = legacyRoundTripPartials(partials);

  const std::vector<std::uint8_t> archiveBytes = mustache::serializeArchivedTemplate(root, partials);
  const mustache::ArchivedTemplate archived = mustache::loadArchivedTemplate(archiveBytes);

  const auto renderOwned = [&](const mustache::RenderLimits& limits) {
    std::string output;
    engine.render(&root, &generated.data, &partials, &output, limits);
    return output;
  };
  const auto renderLegacy = [&](const mustache::RenderLimits& limits) {
    std::string output;
    engine.render(legacyRoot.get(), &generated.data, &legacyPartials, &output, limits);
    return output;
  };
  const auto renderArchived = [&](const mustache::RenderLimits& limits) {
    return mustache::render(archived, generated.data, limits);
  };

  mustache::RenderLimits renderLimits;
  const std::string ownedOutput = renderOwned(renderLimits);
  const std::string legacyOutput = renderLegacy(renderLimits);
  if (legacyOutput != ownedOutput) {
    reportMismatch("legacy", ownedOutput, legacyOutput);
  }

  const std::string archivedOutput = renderArchived(renderLimits);
  if (archivedOutput != ownedOutput) {
    reportMismatch("archived", ownedOutput, archivedOutput);
  }

  renderLimits.maxOutputBytes = ownedOutput.size();
  const std::string exactOwnedOutput = renderOwned(renderLimits);
  const std::string exactLegacyOutput = renderLegacy(renderLimits);
  const std::string exactArchivedOutput = renderArchived(renderLimits);
  if (exactOwnedOutput != ownedOutput) {
    reportMismatch("owned exact output limit", ownedOutput, exactOwnedOutput);
  }
  if (exactLegacyOutput != ownedOutput) {
    reportMismatch("legacy exact output limit", ownedOutput, exactLegacyOutput);
  }
  if (exactArchivedOutput != ownedOutput) {
    reportMismatch("archived exact output limit", ownedOutput, exactArchivedOutput);
  }
  ++coverage.exactOutputLimits;

  if (!ownedOutput.empty()) {
    --renderLimits.maxOutputBytes;
    expectRenderRejected("owned", "Render output byte limit exceeded", renderOwned, renderLimits);
    expectRenderRejected("legacy", "Render output byte limit exceeded", renderLegacy, renderLimits);
    expectRenderRejected("archived", "Render output byte limit exceeded", renderArchived, renderLimits);
    ++coverage.rejectedOutputLimits;
  }
}

void checkPartialAndNodeVisitBoundaries(Coverage& coverage)
{
  mustache::Mustache engine;
  mustache::Node root;
  engine.tokenize("{{>choice}}|{{>fallback}}", &root);

  const std::map<std::string, std::string> rootPartialSources = {{"choice", "root-choice"}, {"fallback", "fallback"}};
  root.partials = tokenizePartials(engine, rootPartialSources);

  const std::map<std::string, std::string> externalPartialSources = {{"choice", "external"}};
  mustache::Node::Partials externalPartials = tokenizePartials(engine, externalPartialSources);
  externalPartials.emplace("fallback", std::unique_ptr<mustache::Node>());
  ++coverage.rootOwnedPartials;
  ++coverage.externalPartialOverrides;

  const std::vector<std::uint8_t> legacyBytes = root.serializeValue();
  std::unique_ptr<mustache::Node> legacyRoot = mustache::Node::unserializeOwned(byteView(legacyBytes));
  // The legacy format serializes nodes rather than a root's partial map. Round
  // trip each partial independently so all nodes still pass through that
  // representation while preserving the renderer's effective partial graph.
  legacyRoot->partials = legacyRoundTripPartials(root.partials);
  mustache::Node::Partials legacyExternalPartials = legacyRoundTripPartials(externalPartials);

  const std::vector<std::uint8_t> archiveBytes = mustache::serializeArchivedTemplate(root, externalPartials);
  const mustache::ArchivedTemplate archived = mustache::loadArchivedTemplate(archiveBytes);
  const mustache::Data data = mustache::Data::null();

  const auto renderOwned = [&](const mustache::RenderLimits& limits) {
    std::string output;
    engine.render(&root, &data, &externalPartials, &output, limits);
    return output;
  };
  const auto renderLegacy = [&](const mustache::RenderLimits& limits) {
    std::string output;
    engine.render(legacyRoot.get(), &data, &legacyExternalPartials, &output, limits);
    return output;
  };
  const auto renderArchived = [&](const mustache::RenderLimits& limits) {
    return mustache::render(archived, data, limits);
  };

  const std::string expected = "external|fallback";
  mustache::RenderLimits limits;
  limits.maxNodeVisits = 8;
  const std::string ownedOutput = renderOwned(limits);
  const std::string legacyOutput = renderLegacy(limits);
  const std::string archivedOutput = renderArchived(limits);
  if (ownedOutput != expected) {
    reportMismatch("owned partial precedence", expected, ownedOutput);
  }
  if (legacyOutput != expected) {
    reportMismatch("legacy partial precedence", expected, legacyOutput);
  }
  if (archivedOutput != expected) {
    reportMismatch("archived partial precedence", expected, archivedOutput);
  }
  ++coverage.exactNodeVisitLimits;

  --limits.maxNodeVisits;
  expectRenderRejected("owned", "Render node visit limit exceeded", renderOwned, limits);
  expectRenderRejected("legacy", "Render node visit limit exceeded", renderLegacy, limits);
  expectRenderRejected("archived", "Render node visit limit exceeded", renderArchived, limits);
  ++coverage.rejectedNodeVisitLimits;
}

void checkCoverage(const Coverage& coverage)
{
  for (std::size_t kind = 0; kind < coverage.fragments.size(); ++kind) {
    if (coverage.fragments[kind] == 0) {
      throw std::runtime_error("a generated template fragment kind was not exercised");
    }
  }
  if (coverage.arrayContexts == 0 || coverage.listContexts == 0 || coverage.emptyLists == 0 ||
      coverage.nonemptyLists == 0 || coverage.embeddedNuls == 0) {
    throw std::runtime_error("the generated data distribution lost a required boundary class");
  }
  if (coverage.variableLambdas == 0 || coverage.sectionLambdas == 0 || coverage.rootOwnedPartials == 0 ||
      coverage.externalPartialOverrides == 0) {
    throw std::runtime_error("the generated semantic distribution lost a required callback or partial class");
  }
  if (coverage.exactOutputLimits == 0 || coverage.rejectedOutputLimits == 0 || coverage.exactNodeVisitLimits == 0 ||
      coverage.rejectedNodeVisitLimits == 0) {
    throw std::runtime_error("the generated limit distribution lost a required boundary class");
  }
}

} // namespace

int main()
{
  static constexpr std::array<std::uint64_t, 4> seeds = {UINT64_C(0x0c1a57a5e1234f01), UINT64_C(0x51a1ed5eed0ddba1),
      UINT64_C(0xa57c0ffee0ddf00d), UINT64_C(0xffff000000000001)};
  Coverage coverage;
  try {
    for (const std::uint64_t seed : seeds) {
      Random random(seed);
      for (std::size_t caseIndex = 0; caseIndex < casesPerSeed; ++caseIndex) {
        const GeneratedCase generated = makeCase(random, caseIndex, coverage);
        try {
          checkCase(generated, coverage);
        } catch (const std::exception&) {
          std::fprintf(stderr, "counterexample: seed=0x%016llx case=%zu\n  template: %s\n",
              static_cast<unsigned long long>(seed), caseIndex, escaped(generated.source).c_str());
          throw;
        }
      }
    }
    checkPartialAndNodeVisitBoundaries(coverage);
    checkCoverage(coverage);
  } catch (const std::exception& exception) {
    std::fprintf(stderr, "render equivalence property test failed: %s\n", exception.what());
    return 1;
  }
  return 0;
}
