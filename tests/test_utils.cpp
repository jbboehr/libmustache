
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

} // namespace

int main()
{
  std::string case1 = "a.b.c";
  std::vector<std::string> result1;
  mustache::explode(".", case1, &result1);
  if (result1.size() != 3) {
    fprintf(stdout, "Failed, expected three parts, got %zu\n", result1.size());
    return 1;
  }
  if (result1[0].compare("a") != 0) {
    fprintf(stdout, "Failed, expected part 1 to be 'a', got '%s'\n", result1[0].c_str());
    return 1;
  }
  if (result1[1].compare("b") != 0) {
    fprintf(stdout, "Failed, expected part 2 to be 'b', got '%s'\n", result1[1].c_str());
    return 1;
  }
  if (result1[2].compare("c") != 0) {
    fprintf(stdout, "Failed, expected part 3 to be 'c', got '%s'\n", result1[2].c_str());
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

  return 0;
}
