#include "mustache_config.h"

#include <cstdio>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "mustache.hpp"

namespace {

int failures = 0;

std::string escapeBytes(const std::string& value)
{
  std::string escaped;
  char buffer[5];
  for( std::size_t i = 0; i < value.size(); ++i ) {
    const unsigned char chr = static_cast<unsigned char>(value[i]);
    switch( chr ) {
      case '\\':
        escaped.append("\\\\");
        break;
      case '"':
        escaped.append("\\\"");
        break;
      case '\n':
        escaped.append("\\n");
        break;
      case '\r':
        escaped.append("\\r");
        break;
      case '\t':
        escaped.append("\\t");
        break;
      default:
        if( chr < 0x20 || chr >= 0x7f ) {
          std::snprintf(buffer, sizeof(buffer), "\\x%02x", chr);
          escaped.append(buffer);
        } else {
          escaped.push_back(static_cast<char>(chr));
        }
        break;
    }
  }
  return escaped;
}

void expectEqual(
    const char * label, const std::string& actual, const std::string& expected)
{
  if( actual == expected ) {
    return;
  }

  std::fprintf(stderr, "%s failed\n  expected: \"%s\"\n  actual:   \"%s\"\n",
      label, escapeBytes(expected).c_str(), escapeBytes(actual).c_str());
  ++failures;
}

void expect(bool condition, const char * message)
{
  if( !condition ) {
    std::fprintf(stderr, "%s\n", message);
    ++failures;
  }
}

std::string renderScalar(mustache::Data * data, const std::string& name)
{
  const std::string lookup = name.empty() ? "." : name;
  std::string tmpl = "{{" + lookup + "}}|{{#" + lookup +
      "}}Y{{/" + lookup + "}}|{{^" + lookup + "}}N{{/" + lookup + "}}";
  mustache::Mustache mustache;
  mustache::Node root;
  mustache.tokenize(&tmpl, &root);

  std::string output;
  mustache.render(&root, data, NULL, &output);
  return output;
}

std::string renderUnescaped(mustache::Data * data, const std::string& name)
{
  const std::string lookup = name.empty() ? "." : name;
  std::string tmpl = "{{{" + lookup + "}}}";
  mustache::Mustache mustache;
  mustache::Node root;
  mustache.tokenize(&tmpl, &root);

  std::string output;
  mustache.render(&root, data, NULL, &output);
  return output;
}

void expectDirectString(
    const char * label, const std::string& value, const std::string& expected)
{
  mustache::Data data = mustache::Data::string(value);
  expectEqual(label, renderScalar(&data, ""), expected);
}

void checkRejectionMessage(const char * label,
    const mustache::Exception& exception, const char * expectedMessage)
{
  if( expectedMessage != NULL ) {
    expectEqual(label, exception.what(), expectedMessage);
  }
}

void expectJSONRejected(const char * label, const char * input,
    const char * expectedMessage = NULL)
{
  bool rejected = false;
  try {
    mustache::Data::fromJSON(input);
  } catch( const mustache::Exception& exception ) {
    rejected = true;
    checkRejectionMessage(label, exception, expectedMessage);
  }
  if( !rejected ) {
    std::fprintf(stderr, "%s failed: JSON input was accepted\n", label);
    ++failures;
  }
}

void expectJSONRejected(const char * label, std::string_view input,
    const mustache::Data::ParseLimits& limits,
    const char * expectedMessage = NULL)
{
  bool rejected = false;
  try {
    mustache::Data::fromJSON(input, limits);
  } catch( const mustache::Exception& exception ) {
    rejected = true;
    checkRejectionMessage(label, exception, expectedMessage);
  }
  if( !rejected ) {
    std::fprintf(stderr, "%s failed: JSON input was accepted\n", label);
    ++failures;
  }
}

void expectYAMLRejected(const char * label, std::string_view input,
    const mustache::Data::ParseLimits& limits =
        mustache::Data::ParseLimits(),
    const char * expectedMessage = NULL)
{
  bool rejected = false;
  try {
    mustache::Data::fromYAML(input, limits);
  } catch( const mustache::Exception& exception ) {
    rejected = true;
    checkRejectionMessage(label, exception, expectedMessage);
  }
  if( !rejected ) {
    std::fprintf(stderr, "%s failed: YAML input was accepted\n", label);
    ++failures;
  }
}

mustache::Data::ParseLimits exactParseLimits(std::size_t inputBytes)
{
  mustache::Data::ParseLimits limits;
  limits.maxInputBytes = inputBytes;
  limits.maxNestingDepth = 2;
  limits.maxNodes = 2;
  limits.maxStringBytes = 8;
  limits.maxContainerEntries = 1;
  return limits;
}

std::string nestedJSONArray(std::size_t arrays)
{
  std::string input(arrays, '[');
  input.push_back('0');
  input.append(arrays, ']');
  return input;
}

template <typename Callable>
void expectDataException(const char * message, Callable&& callable)
{
  bool rejected = false;
  try {
    std::forward<Callable>(callable)();
  } catch( const mustache::Exception& ) {
    rejected = true;
  }
  expect(rejected, message);
}

template <typename Callable>
void expectDataExceptionMessage(const char * label,
    const char * expectedMessage, Callable&& callable)
{
  bool rejected = false;
  try {
    std::forward<Callable>(callable)();
  } catch( const mustache::Exception& exception ) {
    rejected = true;
    checkRejectionMessage(label, exception, expectedMessage);
  }
  if( !rejected ) {
    std::fprintf(stderr, "%s failed: no exception was thrown\n", label);
    ++failures;
  }
}

void testDirectData()
{
  mustache::Data none;
  expectEqual("direct null", renderScalar(&none, ""), "||N");

  mustache::Data falseValue = mustache::Data::boolean(false);
  mustache::Data trueValue = mustache::Data::boolean(true);
  mustache::Data integerValue = mustache::Data::integer(42);
  mustache::Data decimalValue = mustache::Data::floating(1.5);
  expectEqual("direct false", renderScalar(&falseValue, ""), "||N");
  expectEqual("direct true", renderScalar(&trueValue, ""), "true|Y|");
  expectEqual("direct integer", renderScalar(&integerValue, ""), "42|Y|");
  expectEqual("direct decimal", renderScalar(&decimalValue, ""), "1.5|Y|");
  expectEqual(
      "direct unescaped decimal", renderUnescaped(&decimalValue, ""), "1.5");

  expectDirectString("direct empty string", "", "||N");
  expectDirectString("direct zero string", "0", "0|Y|");
  expectDirectString("direct false string", "false", "false|Y|");
  expectDirectString("direct true string", "true", "true|Y|");
  expectDirectString("direct integer string", "42", "42|Y|");
  expectDirectString("direct decimal string", "1.50", "1.50|Y|");

  const std::string withNull("a\0b", 3);
  std::string expectedWithNull(withNull);
  expectedWithNull.append("|Y|");
  expectDirectString(
      "direct string with embedded NUL", withNull, expectedWithNull);

  mustache::Data object = mustache::Data::object({
      {"name", mustache::Data::string("Ada")},
      {"values", mustache::Data::array({
          mustache::Data::integer(1), mustache::Data::integer(2)})}
  });
  const mustache::Data * values = object.find("values");
  expect(values != NULL && values->arrayItems().size() == 2 &&
          values->arrayItems()[1].integerValue() == 2,
      "owned Data object/array factories lost nested values");

  bool rejected = false;
  try {
    mustache::Data::floating(
        std::numeric_limits<double>::infinity());
  } catch( const mustache::Exception& ) {
    rejected = true;
  }
  expect(rejected, "non-finite floating-point Data was accepted");

  mustache::Data scalar = mustache::Data::string("value");
  expect(scalar.find("missing") == NULL,
      "finding a member on scalar Data did not return null");
  expectDataException("setting a member on scalar Data was accepted",
      [&scalar]() { scalar.set("key", mustache::Data::null()); });
  expectDataException("appending to scalar Data was accepted",
      [&scalar]() { scalar.push_back(mustache::Data::null()); });
  mustache::Data array = mustache::Data::array();
  expectDataException("array Data converted to a scalar string",
      [&array]() { static_cast<void>(array.toString()); });
}

void testJSONData()
{
  const char json[] =
      "{"
      "\"nullValue\":null,"
      "\"falseValue\":false,"
      "\"trueValue\":true,"
      "\"zeroValue\":0,"
      "\"integerValue\":42,"
      "\"minimumInteger\":-9223372036854775808,"
      "\"maximumInteger\":9223372036854775807,"
      "\"negativeIntegerZero\":-0,"
      "\"decimalValue\":1.50,"
      "\"pointOne\":0.1,"
      "\"wholeDecimal\":1.0,"
      "\"exponent\":1e30,"
      "\"negativeZero\":-0.0,"
      "\"overflowingDecimal\":1e999,"
      "\"stringValue\":\"false\","
      "\"emptyValue\":\"\""
      "}";
  std::unique_ptr<mustache::Data> data(mustache::Data::createFromJSON(json));

  expect(data->find("nullValue")->type() == mustache::Data::TypeNone &&
          data->find("falseValue")->type() == mustache::Data::TypeBoolean &&
          data->find("integerValue")->type() == mustache::Data::TypeInteger &&
          data->find("decimalValue")->type() == mustache::Data::TypeDouble &&
          data->find("stringValue")->type() == mustache::Data::TypeString,
      "JSON conversion collapsed typed scalars");

  expectEqual("JSON null", renderScalar(data.get(), "nullValue"), "||N");
  expectEqual("JSON false", renderScalar(data.get(), "falseValue"), "||N");
  expectEqual("JSON true", renderScalar(data.get(), "trueValue"), "true|Y|");
  expectEqual("JSON zero", renderScalar(data.get(), "zeroValue"), "0|Y|");
  expectEqual(
      "JSON integer", renderScalar(data.get(), "integerValue"), "42|Y|");
  expectEqual("JSON minimum integer",
      renderScalar(data.get(), "minimumInteger"),
      "-9223372036854775808|Y|");
  expectEqual("JSON maximum integer",
      renderScalar(data.get(), "maximumInteger"),
      "9223372036854775807|Y|");
  expectEqual("JSON negative integer zero",
      renderScalar(data.get(), "negativeIntegerZero"), "0|Y|");
  expectEqual("JSON decimal spelling",
      renderScalar(data.get(), "decimalValue"), "1.50|Y|");
  expectEqual("JSON point-one spelling",
      renderScalar(data.get(), "pointOne"), "0.1|Y|");
  expectEqual("JSON whole-decimal spelling",
      renderScalar(data.get(), "wholeDecimal"), "1.0|Y|");
  expectEqual("JSON exponent spelling",
      renderScalar(data.get(), "exponent"), "1e30|Y|");
  expectEqual("JSON negative-zero spelling",
      renderScalar(data.get(), "negativeZero"), "-0.0|Y|");
  expectEqual("JSON overflowing-decimal spelling",
      renderScalar(data.get(), "overflowingDecimal"), "1e999|Y|");
  expectEqual("JSON unescaped decimal spelling",
      renderUnescaped(data.get(), "decimalValue"), "1.50");
  mustache::Data copiedDecimal = *data->find("decimalValue");
  expectEqual("copied JSON decimal spelling",
      renderScalar(&copiedDecimal, ""), "1.50|Y|");
  expectEqual(
      "JSON string", renderScalar(data.get(), "stringValue"), "false|Y|");
  expectEqual(
      "JSON empty string", renderScalar(data.get(), "emptyValue"), "||N");

  std::unique_ptr<mustache::Data> topLevelNull(
      mustache::Data::createFromJSON("null"));
  expect(topLevelNull->type() == mustache::Data::TypeNone,
      "JSON top-level null did not produce typed null data");
  expectEqual("JSON top-level null", renderScalar(topLevelNull.get(), ""),
      "||N");

  mustache::Data whitespaceNull = mustache::Data::fromJSON("null \t\r\n");
  expect(whitespaceNull.type() == mustache::Data::TypeNone,
      "JSON top-level null with whitespace was rejected");

  expectJSONRejected("JSON null suffix", "nullx");
  expectJSONRejected("JSON second value after null", "null true");
  expectJSONRejected("JSON object suffix", "{}garbage");
  expectJSONRejected("JSON array suffix", "[1,2] trailing");
  expectJSONRejected("JSON unsigned integer outside signed range",
      "{\"value\":9223372036854775808}");
  expectJSONRejected("JSON integer below signed range",
      "{\"value\":-9223372036854775809}");
  expectJSONRejected("JSON integer above unsigned range",
      "{\"value\":18446744073709551616}");
}

void testYAMLData()
{
  const char yaml[] =
      "nullValue: null\n"
      "falseValue: false\n"
      "trueValue: true\n"
      "zeroValue: 0\n"
      "integerValue: 42\n"
      "decimalValue: 1.50\n"
      "stringValue: \"false\"\n"
      "emptyValue: \"\"\n";
  std::unique_ptr<mustache::Data> data(mustache::Data::createFromYAML(yaml));

  expect(data->find("falseValue")->type() == mustache::Data::TypeString &&
          data->find("integerValue")->type() == mustache::Data::TypeString,
      "YAML scalar compatibility unexpectedly changed type behavior");

  expectEqual(
      "YAML null", renderScalar(data.get(), "nullValue"), "null|Y|");
  expectEqual(
      "YAML false", renderScalar(data.get(), "falseValue"), "false|Y|");
  expectEqual(
      "YAML true", renderScalar(data.get(), "trueValue"), "true|Y|");
  expectEqual("YAML zero", renderScalar(data.get(), "zeroValue"), "0|Y|");
  expectEqual(
      "YAML integer", renderScalar(data.get(), "integerValue"), "42|Y|");
  expectEqual(
      "YAML decimal", renderScalar(data.get(), "decimalValue"), "1.50|Y|");
  expectEqual(
      "YAML string", renderScalar(data.get(), "stringValue"), "false|Y|");
  expectEqual(
      "YAML empty string", renderScalar(data.get(), "emptyValue"), "||N");
}

void testParseLimits()
{
  mustache::Data::ParseLimits defaults;
  expect(defaults.maxInputBytes == 64 * 1024 * 1024,
      "default data input-byte limit changed");
  expect(defaults.maxNestingDepth == 32,
      "default data nesting limit changed");
  expect(defaults.maxNodes == 100000,
      "default data node-count limit changed");
  expect(defaults.maxStringBytes == 64 * 1024 * 1024,
      "default data string-byte limit changed");
  expect(defaults.maxContainerEntries == 100000,
      "default data container-entry limit changed");

  const std::string json = "{\"key\":\"value\"}";
  mustache::Data::ParseLimits limits = exactParseLimits(json.size());
  mustache::Data parsedJSON = mustache::Data::fromJSON(
      std::string_view(json), limits);
  expect(parsedJSON.find("key") != NULL &&
          parsedJSON.find("key")->stringValue() == "value",
      "exact JSON parse limits rejected valid input");

  mustache::Data charJSON = mustache::Data::fromJSON(
      json.c_str(), exactParseLimits(json.size()));
  std::unique_ptr<mustache::Data> pointerJSON(
      mustache::Data::createFromJSON(
          json.c_str(), exactParseLimits(json.size())));
  expect(charJSON.find("key") != NULL && pointerJSON->find("key") != NULL,
      "limit-aware JSON compatibility overloads lost object data");
  const char boundedJSON[] = {'{', '"', 'k', '"', ':', '1', '}', 'x'};
  mustache::Data viewJSON = mustache::Data::fromJSON(
      std::string_view(boundedJSON, 7));
  expect(viewJSON.find("k") != NULL &&
          viewJSON.find("k")->integerValue() == 1,
      "default-limit JSON string-view overload read beyond its bound");

  expectDataExceptionMessage("null limit-aware JSON input",
      "Missing JSON data", [&defaults]() {
        static_cast<void>(mustache::Data::fromJSON(NULL, defaults));
      });
  expectDataExceptionMessage("null limit-aware JSON factory input",
      "Missing JSON data", [&defaults]() {
        std::unique_ptr<mustache::Data> value(
            mustache::Data::createFromJSON(NULL, defaults));
      });

  limits = exactParseLimits(json.size());
  limits.maxInputBytes = json.size() - 1;
  expectJSONRejected("JSON input-byte limit", json, limits,
      "JSON input byte limit exceeded");
  limits = defaults;
  limits.maxInputBytes = 0;
  expectJSONRejected("zero JSON input-byte limit", "null", limits,
      "JSON input byte limit exceeded");
  limits = exactParseLimits(json.size());
  limits.maxNestingDepth = 1;
  expectJSONRejected("JSON nesting limit", json, limits,
      "JSON nesting limit exceeded");
  limits = exactParseLimits(json.size());
  limits.maxNodes = 1;
  expectJSONRejected("JSON node-count limit", json, limits,
      "JSON node count limit exceeded");
  limits = exactParseLimits(json.size());
  limits.maxStringBytes = 7;
  expectJSONRejected("JSON string-byte limit", json, limits,
      "JSON string byte limit exceeded");
  limits = exactParseLimits(json.size());
  limits.maxContainerEntries = 0;
  expectJSONRejected("JSON container-entry limit", json, limits,
      "JSON container entry limit exceeded");

  limits = defaults;
  limits.maxNodes = 0;
  expectJSONRejected("zero JSON node-count limit", "null", limits,
      "JSON node count limit exceeded");

  mustache::Data defaultDepthJSON = mustache::Data::fromJSON(
      nestedJSONArray(31));
  expect(defaultDepthJSON.type() == mustache::Data::TypeArray,
      "default JSON nesting limit rejected its exact boundary");
  expectJSONRejected("default JSON nesting boundary",
      nestedJSONArray(32), defaults, "JSON nesting limit exceeded");

  const std::string maximumDepthJSON = nestedJSONArray(255);
  limits = defaults;
  limits.maxNestingDepth = 1000;
  limits.maxNodes = 1000;
  limits.maxContainerEntries = 1000;
  mustache::Data maximumDepthData = mustache::Data::fromJSON(
      maximumDepthJSON, limits);
  expect(maximumDepthData.type() == mustache::Data::TypeArray,
      "JSON implementation nesting ceiling rejected its exact limit");
  expectJSONRejected("JSON implementation nesting ceiling",
      nestedJSONArray(256), limits, "JSON nesting limit exceeded");

  expectJSONRejected("JSON object key starting with escaped NUL",
      "{\"\\u0000AAAA\":0}", defaults,
      "JSON object keys may not contain NUL");
  expectJSONRejected("JSON object key containing escaped NUL",
      "{\"a\\u0000b\":0}", defaults,
      "JSON object keys may not contain NUL");
  limits = defaults;
  limits.maxStringBytes = 0;
  expectJSONRejected("JSON NUL key cannot bypass a zero string budget",
      "{\"\\u0000AAAA\":0}", limits,
      "JSON object keys may not contain NUL");

  const std::string escapedNulValue = "{\"key\":\"a\\u0000b\"}";
  limits = exactParseLimits(escapedNulValue.size());
  limits.maxStringBytes = 6;
  mustache::Data nulValue = mustache::Data::fromJSON(
      escapedNulValue, limits);
  const mustache::Data * nulString = nulValue.find("key");
  expect(nulString != NULL && nulString->stringValue().size() == 3 &&
          nulString->stringValue()[0] == 'a' &&
          nulString->stringValue()[1] == '\0' &&
          nulString->stringValue()[2] == 'b',
      "JSON escaped NUL string value was not preserved by length");

  const std::string escapedUnicode =
      "{\"\\u20ac\":\"\\ud83d\\ude00\"}";
  limits = exactParseLimits(escapedUnicode.size());
  limits.maxStringBytes = 7;
  mustache::Data unicodeData = mustache::Data::fromJSON(
      escapedUnicode, limits);
  expect(unicodeData.objectItems().size() == 1,
      "decoded JSON Unicode byte budget rejected its exact boundary");
  limits.maxStringBytes = 6;
  expectJSONRejected("decoded JSON Unicode string-byte limit",
      escapedUnicode, limits, "JSON string byte limit exceeded");

  const std::string yaml = "key: value\n";
  limits = exactParseLimits(yaml.size());
  mustache::Data parsedYAML = mustache::Data::fromYAML(
      std::string_view(yaml), limits);
  expect(parsedYAML.find("key") != NULL &&
          parsedYAML.find("key")->stringValue() == "value",
      "exact YAML parse limits rejected valid input");

  mustache::Data charYAML = mustache::Data::fromYAML(
      yaml.c_str(), exactParseLimits(yaml.size()));
  std::unique_ptr<mustache::Data> pointerYAML(
      mustache::Data::createFromYAML(
          yaml.c_str(), exactParseLimits(yaml.size())));
  expect(charYAML.find("key") != NULL && pointerYAML->find("key") != NULL,
      "limit-aware YAML compatibility overloads lost object data");
  const char boundedYAML[] = {'k', ':', ' ', 'v', '\n', 'x'};
  mustache::Data viewYAML = mustache::Data::fromYAML(
      std::string_view(boundedYAML, 5));
  expect(viewYAML.find("k") != NULL &&
          viewYAML.find("k")->stringValue() == "v",
      "default-limit YAML string-view overload read beyond its bound");

  expectDataExceptionMessage("null limit-aware YAML input",
      "Missing YAML data", [&defaults]() {
        static_cast<void>(mustache::Data::fromYAML(NULL, defaults));
      });
  expectDataExceptionMessage("null limit-aware YAML factory input",
      "Missing YAML data", [&defaults]() {
        std::unique_ptr<mustache::Data> value(
            mustache::Data::createFromYAML(NULL, defaults));
      });

  limits = exactParseLimits(yaml.size());
  limits.maxInputBytes = yaml.size() - 1;
  expectYAMLRejected("YAML input-byte limit", yaml, limits,
      "YAML input byte limit exceeded");
  limits = defaults;
  limits.maxInputBytes = 0;
  expectYAMLRejected("zero YAML input-byte limit", "null", limits,
      "YAML input byte limit exceeded");
  limits = exactParseLimits(yaml.size());
  limits.maxNestingDepth = 1;
  expectYAMLRejected("YAML nesting limit", yaml, limits,
      "YAML nesting limit exceeded");
  limits = exactParseLimits(yaml.size());
  limits.maxNodes = 1;
  expectYAMLRejected("YAML node-count limit", yaml, limits,
      "YAML node count limit exceeded");
  limits = exactParseLimits(yaml.size());
  limits.maxStringBytes = 7;
  expectYAMLRejected("YAML string-byte limit", yaml, limits,
      "YAML string byte limit exceeded");
  limits = exactParseLimits(yaml.size());
  limits.maxContainerEntries = 0;
  expectYAMLRejected("YAML container-entry limit", yaml, limits,
      "YAML container entry limit exceeded");
  limits = defaults;
  limits.maxNodes = 0;
  expectYAMLRejected("zero YAML node-count limit", "null", limits,
      "YAML node count limit exceeded");

  const std::string jsonWithNull("{\"key\":1}\0{}", 12);
  expectJSONRejected("length-aware JSON embedded NUL", jsonWithNull,
      defaults, "JSON input contains NUL byte");
  const std::string yamlWithNull("key: value\0next: value\n", 23);
  expectYAMLRejected("length-aware YAML embedded NUL", yamlWithNull,
      defaults, "YAML input contains NUL byte");
  expectYAMLRejected("multiple YAML documents",
      "---\nfirst: value\n---\nsecond: value\n", defaults,
      "Multiple YAML documents are not supported");
  expectYAMLRejected("trailing empty YAML document",
      "key: value\n---\n", defaults,
      "Multiple YAML documents are not supported");
  expectYAMLRejected("malformed trailing YAML content",
      "key: value\n...\n[unclosed\n", defaults,
      "Invalid trailing YAML content");

  mustache::Data endedYAML = mustache::Data::fromYAML(
      "key: value\n...\n");
  mustache::Data directedYAML = mustache::Data::fromYAML(
      "%YAML 1.2\n---\nkey: value\n...\n");
  expect(endedYAML.find("key") != NULL &&
          directedYAML.find("key") != NULL,
      "valid YAML document markers or directives were rejected");

  const std::string deepYAML =
      std::string(1000, '[') + std::string(1000, ']');
  expectYAMLRejected("YAML preflight nesting limit", deepYAML, defaults,
      "YAML nesting limit exceeded");

  expectYAMLRejected("recursive YAML mapping alias",
      "root: &root\n  self: *root\n");
  expectYAMLRejected("recursive YAML sequence alias", "&loop [*loop]\n");

  const std::string aliases =
      "base: &base [one, two]\n"
      "copy: *base\n";
  mustache::Data aliasData = mustache::Data::fromYAML(aliases);
  expect(aliasData.find("base") != NULL && aliasData.find("copy") != NULL &&
          aliasData.find("base")->arrayItems().size() == 2 &&
          aliasData.find("copy")->arrayItems().size() == 2,
      "non-recursive YAML aliases were not expanded");

  limits = defaults;
  limits.maxNodes = 6;
  expectYAMLRejected("YAML alias node amplification", aliases, limits);
  limits = defaults;
  limits.maxContainerEntries = 5;
  expectYAMLRejected("YAML alias container amplification", aliases, limits);
  limits = defaults;
  limits.maxStringBytes = 19;
  expectYAMLRejected("YAML alias string amplification", aliases, limits);
}

} // namespace

int main()
{
  testDirectData();
  testJSONData();
  testYAMLData();
  testParseLimits();
  return failures == 0 ? 0 : 1;
}
