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

void expectJSONRejected(const char * label, const char * input)
{
  bool rejected = false;
  try {
    mustache::Data::fromJSON(input);
  } catch( const mustache::Exception& ) {
    rejected = true;
  }
  if( !rejected ) {
    std::fprintf(stderr, "%s failed: JSON input was accepted\n", label);
    ++failures;
  }
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

} // namespace

int main()
{
  testDirectData();
  testJSONData();
  testYAMLData();
  return failures == 0 ? 0 : 1;
}
