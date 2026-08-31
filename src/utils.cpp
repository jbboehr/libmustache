
#include "utils.hpp"

namespace mustache {

namespace {

bool aliasesElement(const std::string& value, const std::vector<std::string>& values)
{
  for (const std::string& candidate : values) {
    if (&candidate == &value) {
      return true;
    }
  }
  return false;
}

} // namespace

void stripWhitespace(std::string& str, std::string_view chars)
{
  std::string tmp;
  for (const char chr : str) {
    std::size_t found = chars.find(chr);
    if (found == std::string::npos) {
      tmp += chr;
    }
  }
  str.swap(tmp);
}

void trimDecimal(std::string& str)
{
  if (str.length() < 20) {
    std::string::size_type found = str.find_first_not_of("0123456789.");
    if (found == std::string::npos) {
      trimRight(str, "0");
    }
  }
}

void trimRight(std::string& str, std::string_view trimChars)
{
  std::string::size_type pos = str.find_last_not_of(trimChars);
  str.erase(pos + 1);
}

void trimLeft(std::string& str, std::string_view trimChars)
{
  std::string::size_type pos = str.find_first_not_of(trimChars);
  str.erase(0, pos);
}

void trim(std::string& str, std::string_view trimChars)
{
  trimRight(str, trimChars);
  trimLeft(str, trimChars);
}

void htmlspecialchars(std::string * str)
{
  std::string tmp;
  tmp.reserve(str->size());
  htmlspecialchars_append(*str, &tmp);
  str->swap(tmp);
}

void htmlspecialchars_append(std::string * str, std::string * buf)
{
  htmlspecialchars_append(*str, buf);
}

void htmlspecialchars_append(const std::string& str, std::string * buf)
{
  if (&str == buf) {
    const std::string stableInput = str;
    htmlspecialchars_append(stableInput, buf);
    return;
  }

  for (const char chr : str) {
    switch (chr) {
      case '&':
        buf->append("&amp;");
        break;
      case '"':
        buf->append("&quot;");
        break;
      case '\'':
        buf->append("&#039;");
        break;
      case '<':
        buf->append("&lt;");
        break;
      case '>':
        buf->append("&gt;");
        break;
      default:
        buf->append(1, chr);
        break;
    }
  }
}

void explode(const std::string& delimiter, const std::string& str, std::vector<std::string> * arr)
{
  if (delimiter.empty()) {
    return;
  }

  const std::string stableDelimiter = delimiter;
  const bool inputAliasesOutput = aliasesElement(str, *arr);
  const std::string stableInput = inputAliasesOutput ? str : std::string();
  const std::string& input = inputAliasesOutput ? stableInput : str;
  std::string::size_type start = 0;
  std::string::size_type separator = input.find(stableDelimiter, start);
  while (separator != std::string::npos) {
    arr->push_back(input.substr(start, separator - start));
    start = separator + stableDelimiter.size();
    separator = input.find(stableDelimiter, start);
  }
  arr->push_back(input.substr(start));
}

void stringTok(const std::string& str, std::string_view delimiters, std::vector<std::string> * tokens)
{
  const bool inputAliasesOutput = aliasesElement(str, *tokens);
  const std::string stableInput = inputAliasesOutput ? str : std::string();
  const std::string& input = inputAliasesOutput ? stableInput : str;
  const std::string stableDelimiters(delimiters);

  // Skip delimiters at beginning.
  std::string::size_type lastPos = input.find_first_not_of(stableDelimiters, 0);

  // Find first "non-delimiter".
  std::string::size_type pos = input.find_first_of(stableDelimiters, lastPos);

  while (std::string::npos != pos || std::string::npos != lastPos) {
    // Found a token, add it to the vector.
    tokens->push_back(input.substr(lastPos, pos - lastPos));
    // Skip delimiters.  Note the "not_of"
    lastPos = input.find_first_not_of(stableDelimiters, pos);
    // Find next "non-delimiter"
    pos = input.find_first_of(stableDelimiters, lastPos);
  }
}

} // namespace mustache
