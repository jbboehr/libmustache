#include "mustache_config.h"

#include <cstdio>
#include <locale>
#include <memory>
#include <sstream>
#include <string>

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

void expectDirectString(
    const char * label, const std::string& value, const std::string& expected)
{
  mustache::Data data(mustache::Data::TypeString,
      static_cast<int>(value.size()));
  data.val->assign(value);
  expectEqual(label, renderScalar(&data, ""), expected);
}

void expectJSONDecimal(const std::string& actual, double expected)
{
  const std::string::size_type separator = actual.find('|');
  if( separator == std::string::npos || actual.substr(separator) != "|Y|" ) {
    std::fprintf(stderr,
        "JSON decimal failed\n  expected: numeric interpolation followed by "
        "\"|Y|\"\n  actual:   \"%s\"\n",
        escapeBytes(actual).c_str());
    ++failures;
    return;
  }

  const std::string number = actual.substr(0, separator);
  std::istringstream stream(number);
  stream.imbue(std::locale::classic());
  double parsed = 0.0;
  char trailing = '\0';
  if( !(stream >> parsed) || (stream >> trailing) || parsed != expected ) {
    std::fprintf(stderr,
        "JSON decimal failed\n  expected: numeric value %.17g\n  actual:   "
        "\"%s\"\n", expected, escapeBytes(number).c_str());
    ++failures;
  }
}

void testDirectData()
{
  mustache::Data none;
  expectEqual("direct null", renderScalar(&none, ""), "||N");

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
      "\"stringValue\":\"false\","
      "\"emptyValue\":\"\""
      "}";
  std::unique_ptr<mustache::Data> data(mustache::Data::createFromJSON(json));

  expectEqual("JSON null", renderScalar(data.get(), "nullValue"), "||N");
  expectEqual("JSON false", renderScalar(data.get(), "falseValue"), "||N");
  expectEqual("JSON true", renderScalar(data.get(), "trueValue"), "true|Y|");
  expectEqual("JSON zero", renderScalar(data.get(), "zeroValue"), "0|Y|");
  expectEqual(
      "JSON integer", renderScalar(data.get(), "integerValue"), "42|Y|");
  // json-c versions differ in whether they preserve a double's lexical form.
  // The adapter contract here is its numeric value and truthiness, not the
  // dependency's chosen decimal spelling.
  expectJSONDecimal(renderScalar(data.get(), "decimalValue"), 1.5);
  expectEqual(
      "JSON string", renderScalar(data.get(), "stringValue"), "false|Y|");
  expectEqual(
      "JSON empty string", renderScalar(data.get(), "emptyValue"), "||N");

  try {
    std::unique_ptr<mustache::Data> topLevelNull(
        mustache::Data::createFromJSON("null"));
    std::fprintf(stderr,
        "JSON top-level null failed\n  expected: Invalid JSON data exception\n");
    ++failures;
  } catch( const mustache::Exception& ) {
    // json-c represents a valid top-level null with a null pointer. The current
    // adapter consequently treats it as invalid input; pin that behavior until
    // the value-model migration changes it deliberately.
  }
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
