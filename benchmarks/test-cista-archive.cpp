#include "cista-archive.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

class FixedLambda final : public mustache::Lambda {
  public:
    std::string invoke() override
    {
      return "lambda";
    }
};

void expect(bool condition, const char * message)
{
  if (!condition) {
    throw std::runtime_error(message);
  }
}

mustache::Data makeData()
{
  mustache::Data first = mustache::Data::object();
  first.set("name", mustache::Data::string("<first>'"));
  first.set("category", mustache::Data::object().set("slug", mustache::Data::string("one")));
  first.set("visible", mustache::Data::boolean(true));

  mustache::Data second = mustache::Data::object();
  second.set("name", mustache::Data::string("second"));
  second.set("category", mustache::Data::object().set("slug", mustache::Data::string("two")));
  second.set("visible", mustache::Data::boolean(false));

  mustache::Data products = mustache::Data::array();
  products.push_back(std::move(first));
  products.push_back(std::move(second));

  mustache::Data root = mustache::Data::object();
  root.set("products", std::move(products));
  return root;
}

void expectRejected(std::string_view bytes, const mustache::Data& data, const char * description)
{
  try {
    mustache_benchmark::renderCistaArchive(bytes, data);
  } catch (const std::exception&) {
    return;
  }
  throw std::runtime_error(std::string("invalid Cista archive was accepted: ") + description);
}

void expectRejected(std::string_view bytes, const mustache::Data& data,
    const mustache_benchmark::CistaArchiveLimits& archiveLimits, const mustache::RenderLimits& renderLimits,
    const char * description)
{
  try {
    mustache_benchmark::renderCistaArchive(bytes, data, archiveLimits, renderLimits);
  } catch (const std::exception&) {
    return;
  }
  throw std::runtime_error(std::string("limited Cista archive render was accepted: ") + description);
}

template <typename Operation> void expectOperationRejected(Operation&& operation, const char * description)
{
  try {
    operation();
  } catch (const std::exception&) {
    return;
  }
  throw std::runtime_error(std::string("invalid Cista archive operation was accepted: ") + description);
}

} // namespace

int main()
{
  try {
    mustache::Mustache engine;
    mustache::Node root;
    engine.tokenize("{{#products}}\n  {{> card}}\n{{/products}}\n", &root);

    mustache::Node::Partials partials;
    mustache::Node card;
    engine.tokenize("<p class=\"{{category.slug}}\">{{name}}</p>{{^visible}}hidden{{/visible}}\n", &card);
    partials.emplace("card", std::make_unique<mustache::Node>(std::move(card)));

    const mustache::Data data = makeData();
    std::string expected;
    engine.render(&root, &data, &partials, &expected);

    const std::vector<std::uint8_t> archive = mustache_benchmark::serializeCistaArchive(root, partials);
    expect(archive == mustache_benchmark::serializeCistaArchive(root, partials),
        "Cista archive serialization is not deterministic");
    const std::string_view bytes(reinterpret_cast<const char *>(archive.data()), archive.size());
    const std::string actual = mustache_benchmark::renderCistaArchive(bytes, data);
    expect(actual == expected, "Cista archive rendering differs from Node rendering");

    mustache::Node invalidType;
    invalidType.type = static_cast<mustache::Node::Type>(65537);
    expectOperationRejected(
        [&invalidType]() {
          (void)mustache_benchmark::serializeCistaArchive(invalidType);
        },
        "node type narrowed before validation");

    mustache_benchmark::CistaArchiveLimits serializationLimits;
    serializationLimits.maxInputBytes = archive.size() - 1;
    expectOperationRejected(
        [&root, &partials, &serializationLimits]() {
          (void)mustache_benchmark::serializeCistaArchive(root, partials, serializationLimits);
        },
        "serialized output exceeds the paired reader limit");

    std::string nulSource("before\0after ", 13);
    nulSource.append("{{products}}");
    mustache::Node nulRoot;
    engine.tokenize(std::string_view(nulSource), &nulRoot);
    std::string expectedNul;
    engine.render(&nulRoot, &data, nullptr, &expectedNul);
    const std::vector<std::uint8_t> nulArchive = mustache_benchmark::serializeCistaArchive(nulRoot);
    const std::string actualNul = mustache_benchmark::renderCistaArchive(
        std::string_view(reinterpret_cast<const char *>(nulArchive.data()), nulArchive.size()), data);
    expect(actualNul == expectedNul, "Cista archive did not preserve embedded NUL bytes");

    mustache::Node nestedRoot;
    engine.tokenize("  {{> outer}}\n", &nestedRoot);
    mustache::Node::Partials nestedPartials;
    std::unique_ptr<mustache::Node> outer = std::make_unique<mustache::Node>();
    engine.tokenize("{{> inner}}\n", outer.get());
    nestedPartials.emplace("outer", std::move(outer));
    std::unique_ptr<mustache::Node> inner = std::make_unique<mustache::Node>();
    engine.tokenize("x\ny\n", inner.get());
    nestedPartials.emplace("inner", std::move(inner));
    std::string expectedNested;
    engine.render(&nestedRoot, &data, &nestedPartials, &expectedNested);
    const std::vector<std::uint8_t> nestedArchive =
        mustache_benchmark::serializeCistaArchive(nestedRoot, nestedPartials);
    const std::string actualNested = mustache_benchmark::renderCistaArchive(
        std::string_view(reinterpret_cast<const char *>(nestedArchive.data()), nestedArchive.size()), data);
    expect(actualNested == expectedNested, "Cista archive lost inherited partial indentation");

    mustache::Node semanticRoot;
    engine.tokenize(
        "{{#values}}[{{.}}]{{/values}}|{{integer}}|{{floating}}|{{> missing}}|{{=<% %>=}}<%integer%>", &semanticRoot);
    mustache::Data values = mustache::Data::array();
    values.push_back(mustache::Data::string("first"));
    values.push_back(mustache::Data::integer(2));
    mustache::Data semanticData = mustache::Data::object();
    semanticData.set("values", std::move(values));
    semanticData.set("integer", mustache::Data::integer(-7));
    semanticData.set("floating", mustache::Data::floating(1.5));
    std::string expectedSemantics;
    engine.render(&semanticRoot, &semanticData, nullptr, &expectedSemantics);
    const std::vector<std::uint8_t> semanticArchive = mustache_benchmark::serializeCistaArchive(semanticRoot);
    const std::string actualSemantics = mustache_benchmark::renderCistaArchive(
        std::string_view(reinterpret_cast<const char *>(semanticArchive.data()), semanticArchive.size()), semanticData);
    expect(expectedSemantics == "[first][2]|-7|1.5||-7", "ordinary renderer semantic fixture changed unexpectedly");
    expect(actualSemantics == expectedSemantics,
        "Cista archive changed current-context, numeric, delimiter, or missing-partial semantics");

    mustache::Node lambdaRoot;
    engine.tokenize("{{call}}", &lambdaRoot);
    const std::vector<std::uint8_t> lambdaArchive = mustache_benchmark::serializeCistaArchive(lambdaRoot);
    const std::string_view lambdaBytes(reinterpret_cast<const char *>(lambdaArchive.data()), lambdaArchive.size());
    mustache::Data plainCallData = mustache::Data::object();
    plainCallData.set("call", mustache::Data::string("plain"));
    expect(mustache_benchmark::renderCistaArchive(lambdaBytes, plainCallData) == "plain",
        "lambda fixture archive could not render ordinary data");
    mustache::Data lambdaData = mustache::Data::object();
    lambdaData.set("call", mustache::Data::lambda(std::make_unique<FixedLambda>()));
    expectRejected(lambdaBytes, lambdaData, "lambda render");

    mustache::Node inlinePartialRoot;
    engine.tokenize("root", &inlinePartialRoot);
    std::unique_ptr<mustache::Node> ownedInlinePartial = std::make_unique<mustache::Node>();
    engine.tokenize("inline", ownedInlinePartial.get());
    inlinePartialRoot.partials.emplace("inline", std::move(ownedInlinePartial));
    expectOperationRejected(
        [&inlinePartialRoot]() {
          (void)mustache_benchmark::serializeCistaArchive(inlinePartialRoot);
        },
        "inline partial ownership");

    mustache::Node containerRoot;
    engine.tokenize("root", &containerRoot);
    containerRoot.child = std::make_unique<mustache::Node>();
    engine.tokenize("child", containerRoot.child.get());
    expectOperationRejected(
        [&containerRoot]() {
          (void)mustache_benchmark::serializeCistaArchive(containerRoot);
        },
        "container child ownership");

    expectRejected(std::string_view(), data, "empty");
    expectRejected(bytes.substr(0, bytes.size() - 1), data, "truncated");

    std::vector<std::uint8_t> corrupted = archive;
    corrupted[0] ^= std::uint8_t{0xFF};
    expectRejected(
        std::string_view(reinterpret_cast<const char *>(corrupted.data()), corrupted.size()), data, "type hash");

    corrupted = archive;
    corrupted.back() ^= std::uint8_t{0xFF};
    expectRejected(
        std::string_view(reinterpret_cast<const char *>(corrupted.data()), corrupted.size()), data, "integrity");

    std::vector<std::uint8_t> unaligned(archive.size() + 1);
    std::copy(archive.begin(), archive.end(), unaligned.begin() + 1);
    expectRejected(std::string_view(reinterpret_cast<const char *>(unaligned.data() + 1), archive.size()), data,
        "unaligned buffer");

    mustache_benchmark::CistaArchiveLimits archiveLimits;
    archiveLimits.maxInputBytes = archive.size() - 1;
    expectRejected(bytes, data, archiveLimits, mustache::RenderLimits(), "input byte limit");
    archiveLimits = mustache_benchmark::CistaArchiveLimits();
    archiveLimits.maxNodes = 0;
    expectRejected(bytes, data, archiveLimits, mustache::RenderLimits(), "node limit");
    archiveLimits = mustache_benchmark::CistaArchiveLimits();
    archiveLimits.maxDataPartsPerNode = 1;
    expectRejected(bytes, data, archiveLimits, mustache::RenderLimits(), "per-node data-part limit");
    archiveLimits = mustache_benchmark::CistaArchiveLimits();
    archiveLimits.maxDataParts = 1;
    expectRejected(bytes, data, archiveLimits, mustache::RenderLimits(), "aggregate data-part limit");
    archiveLimits = mustache_benchmark::CistaArchiveLimits();
    archiveLimits.maxNestingDepth = 1;
    expectRejected(bytes, data, archiveLimits, mustache::RenderLimits(), "archive nesting limit");
    archiveLimits = mustache_benchmark::CistaArchiveLimits();
    archiveLimits.maxStringBytes = 0;
    expectRejected(bytes, data, archiveLimits, mustache::RenderLimits(), "archive string byte limit");

    mustache::RenderLimits renderLimits;
    renderLimits.maxOutputBytes = 0;
    expectRejected(bytes, data, mustache_benchmark::CistaArchiveLimits(), renderLimits, "output byte limit");
    renderLimits = mustache::RenderLimits();
    renderLimits.maxNodeVisits = 0;
    expectRejected(bytes, data, mustache_benchmark::CistaArchiveLimits(), renderLimits, "node visit limit");
  } catch (const std::exception& exception) {
    std::fprintf(stderr, "cista archive test failed: %s\n", exception.what());
    return 1;
  }
  return 0;
}
