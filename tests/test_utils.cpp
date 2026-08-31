
#include "mustache_config.h"

#include <cstdlib>
#include <cstdio>

#include "utils.hpp"

namespace {

bool expectEqual(const char * testName, const std::string& actual, const std::string& expected)
{
  if (actual == expected) {
    return true;
  }

  fprintf(stdout, "Failed %s, expected %zu bytes, got %zu bytes\n", testName, expected.size(), actual.size());
  return false;
}

bool expectEqual(
    const char * testName, const std::vector<std::string>& actual, const std::vector<std::string>& expected)
{
  if (actual == expected) {
    return true;
  }

  fprintf(stdout, "Failed %s, expected %zu parts, got %zu parts\n", testName, expected.size(), actual.size());
  return false;
}

} // namespace

int main()
{
  std::vector<std::string> parts;
  mustache::explode(".", "a.b.c", &parts);
  if (!expectEqual("basic split", parts, {"a", "b", "c"})) {
    return 1;
  }

  parts = {"existing"};
  mustache::explode("::", "a::::b::", &parts);
  if (!expectEqual("multi-character split", parts, {"existing", "a", "", "b", ""})) {
    return 1;
  }

  parts.clear();
  mustache::explode("aa", "aaa", &parts);
  if (!expectEqual("non-overlapping split", parts, {"", "a"})) {
    return 1;
  }

  parts.clear();
  mustache::explode(".", "", &parts);
  if (!expectEqual("empty input split", parts, {""})) {
    return 1;
  }

  parts = {"existing"};
  mustache::explode("", "ignored", &parts);
  if (!expectEqual("empty delimiter split", parts, {"existing"})) {
    return 1;
  }

  parts = {"."};
  while (parts.size() < parts.capacity()) {
    parts.emplace_back("padding");
  }
  std::vector<std::string> aliasedDelimiterExpected = parts;
  aliasedDelimiterExpected.emplace_back("a");
  aliasedDelimiterExpected.emplace_back("");
  mustache::explode(parts.front(), "a.", &parts);
  if (!expectEqual("aliased delimiter split", parts, aliasedDelimiterExpected)) {
    return 1;
  }

  std::vector<std::string> tokens = {"alpha beta gamma"};
  while (tokens.size() < tokens.capacity()) {
    tokens.emplace_back("padding");
  }
  std::vector<std::string> aliasedTokenInputExpected = tokens;
  aliasedTokenInputExpected.emplace_back("alpha");
  aliasedTokenInputExpected.emplace_back("beta");
  aliasedTokenInputExpected.emplace_back("gamma");
  mustache::stringTok(tokens.front(), " ", &tokens);
  if (!expectEqual("aliased token input", tokens, aliasedTokenInputExpected)) {
    return 1;
  }

  std::vector<std::string> delimiterTokens = {" :;!@#$%^&*()"};
  while (delimiterTokens.size() < delimiterTokens.capacity()) {
    delimiterTokens.emplace_back("padding");
  }
  std::vector<std::string> aliasedTokenDelimiterExpected = delimiterTokens;
  aliasedTokenDelimiterExpected.emplace_back("alpha");
  aliasedTokenDelimiterExpected.emplace_back("beta");
  aliasedTokenDelimiterExpected.emplace_back("gamma");
  mustache::stringTok("alpha beta:gamma", delimiterTokens.front(), &delimiterTokens);
  if (!expectEqual("aliased token delimiter", delimiterTokens, aliasedTokenDelimiterExpected)) {
    return 1;
  }

  parts = {"alpha.beta.gamma"};
  while (parts.size() < parts.capacity()) {
    parts.emplace_back("padding");
  }
  std::vector<std::string> aliasedSplitInputExpected = parts;
  aliasedSplitInputExpected.emplace_back("alpha");
  aliasedSplitInputExpected.emplace_back("beta");
  aliasedSplitInputExpected.emplace_back("gamma");
  mustache::explode(".", parts.front(), &parts);
  if (!expectEqual("aliased split input", parts, aliasedSplitInputExpected)) {
    return 1;
  }

  std::string htmlInput = "plain &\"'<>";
  htmlInput.push_back('\0');
  htmlInput.append("tail");

  std::string escapedHtml = "plain &amp;&quot;&#039;&lt;&gt;";
  escapedHtml.push_back('\0');
  escapedHtml.append("tail");

  std::string inPlace = htmlInput;
  mustache::htmlspecialchars(&inPlace);
  if (!expectEqual("in-place HTML escaping", inPlace, escapedHtml)) {
    return 1;
  }

  std::string appended = "prefix:";
  mustache::htmlspecialchars_append(htmlInput, &appended);
  if (!expectEqual("const HTML escaping append", appended, "prefix:" + escapedHtml)) {
    return 1;
  }

  std::string mutableInput = htmlInput;
  std::string mutableAppended = "prefix:";
  mustache::htmlspecialchars_append(&mutableInput, &mutableAppended);
  if (!expectEqual("mutable HTML escaping append", mutableAppended, "prefix:" + escapedHtml)) {
    return 1;
  }

  std::string mutableSelfAppended(64, 'x');
  mutableSelfAppended.front() = '&';
  mutableSelfAppended.resize(mutableSelfAppended.capacity(), 'x');
  const std::string mutableSelfExpected =
      mutableSelfAppended + "&amp;" + std::string(mutableSelfAppended.size() - 1, 'x');
  mustache::htmlspecialchars_append(&mutableSelfAppended, &mutableSelfAppended);
  if (!expectEqual("mutable self-appending HTML escape", mutableSelfAppended, mutableSelfExpected)) {
    return 1;
  }

  std::string constSelfAppended(64, 'y');
  constSelfAppended.front() = '<';
  constSelfAppended.resize(constSelfAppended.capacity(), 'y');
  const std::string constSelfExpected = constSelfAppended + "&lt;" + std::string(constSelfAppended.size() - 1, 'y');
  const std::string& constSelfInput = constSelfAppended;
  mustache::htmlspecialchars_append(constSelfInput, &constSelfAppended);
  if (!expectEqual("const self-appending HTML escape", constSelfAppended, constSelfExpected)) {
    return 1;
  }

  return 0;
}
