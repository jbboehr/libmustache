// Exercise the fixture adapter used by the specification executable itself.
#define main mustache_spec_main
#include "test_spec.cpp"
#undef main

#include <cstdio>

namespace {

int failures = 0;

class CommaDecimal final : public std::numpunct<char> {
  protected:
    char do_decimal_point() const override
    {
      return ',';
    }
};

void expect(bool condition, const char * message)
{
  if (!condition) {
    std::fprintf(stderr, "%s\n", message);
    ++failures;
  }
}

mustache::Data parseData(std::string_view fixture)
{
  yaml_parser_t parser;
  yaml_document_t document;
  if (!yaml_parser_initialize(&parser)) {
    throw std::bad_alloc();
  }
  std::unique_ptr<yaml_parser_t, decltype(&yaml_parser_delete)> parserCleanup(&parser, &yaml_parser_delete);
  yaml_parser_set_input_string(&parser, reinterpret_cast<const unsigned char *>(fixture.data()), fixture.size());
  const bool loaded = yaml_parser_load(&parser, &document) != 0;
  parserCleanup.reset();
  if (!loaded) {
    throw std::runtime_error("Unable to parse test fixture");
  }
  const std::unique_ptr<yaml_document_t, decltype(&yaml_document_delete)> documentCleanup(
      &document, &yaml_document_delete);
  mustache::Data data;
  mustache_spec_parse_data(&document, yaml_document_get_root_node(&document), &data);
  return data;
}

void checkScalar(const char * fixture, mustache::Data::Type type, const std::string& rendered)
{
  const mustache::Data data = parseData(fixture);
  if (data.type() != type || data.toString() != rendered) {
    std::fprintf(stderr, "Scalar %s: expected type %d and value %s, got type %d and value %s\n", fixture,
        static_cast<int>(type), MustacheSpecTest::escapeForDiagnostic(rendered).c_str(), static_cast<int>(data.type()),
        MustacheSpecTest::escapeForDiagnostic(data.toString()).c_str());
    ++failures;
  }
}

void checkCompleteFixture(
    const char * suite, const char * fixture, mustache::Data::Type dataType, const std::string& expectedOutput)
{
  tests.clear();
  currentSuite = suite;
  parse_file(fixture, std::strlen(fixture));
  expect(tests.size() == 1, "Fixture must produce exactly one specification test");
  if (tests.size() == 1) {
    expect(tests.front()->data.type() == dataType, "Root fixture data type must be preserved");
    expect(tests.front()->expected == expectedOutput, "Expected fixture bytes must be preserved");
    expect(tests.front()->output == expectedOutput, "Rendered fixture bytes must match the independent expectation");
    expect(tests.front()->passed(), "Complete fixture must render its expected bytes");
  }
  tests.clear();
}

void checkFixtures()
{
  using mustache::Data;
  checkScalar("42", Data::TypeInteger, "42");
  checkScalar("-17", Data::TypeInteger, "-17");
  checkScalar("0", Data::TypeInteger, "0");
  checkScalar("9223372036854775807", Data::TypeInteger, "9223372036854775807");
  checkScalar("-9223372036854775808", Data::TypeInteger, "-9223372036854775808");
  checkScalar("1.50", Data::TypeDouble, "1.5");
  checkScalar("1e2", Data::TypeDouble, "100");
  checkScalar("true", Data::TypeBoolean, "true");
  checkScalar("false", Data::TypeBoolean, "");
  checkScalar("null", Data::TypeNone, "");
  checkScalar("\"false\"", Data::TypeString, "false");
  checkScalar("\"0\"", Data::TypeString, "0");
  checkScalar("\"null\"", Data::TypeString, "null");
  checkScalar("\"1.50\"", Data::TypeString, "1.50");
  checkScalar("\"false\\u0000tail\"", Data::TypeString, std::string("false\0tail", 10));

  const Data nested = parseData(R"({"items": [42, 1.5, true, false, null, "0"], "a\u0000b": "tail"})");
  const Data * items = nested.find("items");
  expect(items != nullptr && items->type() == Data::TypeArray, "Nested array must remain an array");
  if (items != nullptr && items->type() == Data::TypeArray) {
    const auto& values = items->arrayItems();
    expect(values.size() == 6, "Nested array must retain every entry");
    if (values.size() == 6) {
      expect(values[0].type() == Data::TypeInteger && values[1].type() == Data::TypeDouble &&
              values[2].type() == Data::TypeBoolean && values[3].type() == Data::TypeBoolean &&
              values[4].type() == Data::TypeNone && values[5].type() == Data::TypeString,
          "Nested scalar types must be preserved");
    }
  }
  const Data * nulKey = nested.find(std::string("a\0b", 3));
  expect(nulKey != nullptr && nulKey->toString() == "tail", "Quoted object keys must retain embedded NUL bytes");

  checkCompleteFixture("interpolation.json", R"({"tests": [{"name": "Root integer", "data": 42,
    "template": "{{.}}", "expected": "42"}]})",
      Data::TypeInteger, "42");
  checkCompleteFixture("sections.json", R"({"tests": [{"name": "Root array", "data": [1, 2],
    "template": "{{#.}}{{.}}{{/.}}", "expected": "12"}]})",
      Data::TypeArray, "12");
  checkCompleteFixture("interpolation.json", R"({"tests": [{"name": "Exact bytes", "data": {"value": "tail"},
    "template": "head\u0000{{>piece}}", "partials": {"piece": "mid\u0000{{value}}"},
    "expected": "head\u0000mid\u0000tail"}]})",
      Data::TypeMap, std::string("head\0mid\0tail", 13));
  checkCompleteFixture("~lambdas.json", R"({"tests": [{"name": "Interpolation", "data": {
    "lambda": {"__tag__": "code", "cpp": "fixture placeholder"}},
    "template": "Hello, {{lambda}}!", "expected": "Hello, world!"}]})",
      Data::TypeMap, "Hello, world!");

  tests.clear();
  currentSuite = "interpolation.json";
  const char metadataFixture[] =
      R"({"tests": [{"name": "name\u0000tail", "desc": "desc\u0000tail", "data": {}, "template": "", "expected": ""}]})";
  parse_file(metadataFixture, sizeof(metadataFixture) - 1);
  expect(tests.size() == 1, "Metadata fixture must produce exactly one specification test");
  if (tests.size() == 1) {
    expect(tests.front()->name == std::string("name\0tail", 9), "Fixture name must retain its full byte length");
    expect(tests.front()->desc == std::string("desc\0tail", 9), "Fixture description must retain its full byte length");
  }
  tests.clear();
}

} // namespace

int main()
{
  const std::locale originalLocale;
  std::locale::global(std::locale(originalLocale, new CommaDecimal()));
  try {
    checkFixtures();
  } catch (const std::exception& error) {
    std::fprintf(stderr, "Fixture test failed: %s\n", error.what());
    ++failures;
  }
  std::locale::global(originalLocale);
  tests.clear();
  return failures == 0 ? 0 : 1;
}
