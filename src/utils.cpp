
#include "utils.hpp"

#include "exception.hpp"

#include <iterator>

namespace mustache {

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
  if (str == nullptr) {
    throw Exception("Missing HTML input");
  }
  std::string tmp;
  tmp.reserve(str->size());
  htmlspecialchars_append(*str, &tmp);
  str->swap(tmp);
}

void htmlspecialchars_append(std::string * str, std::string * buf)
{
  if (str == nullptr) {
    throw Exception("Missing HTML input");
  }
  htmlspecialchars_append(*str, buf);
}

void htmlspecialchars_append(const std::string& str, std::string * buf)
{
  if (buf == nullptr) {
    throw Exception("Missing HTML output");
  }
  if (&str == buf) {
    // Appending to buf can invalidate str because they are the same object.
    const std::string stableInput = str; // NOLINT(performance-unnecessary-copy-initialization)
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
  if (arr == nullptr) {
    throw Exception("Missing split output");
  }
  if (delimiter.empty()) {
    return;
  }

  std::vector<std::string> parts;
  std::string::size_type start = 0;
  std::string::size_type separator = str.find(delimiter, start);
  while (separator != std::string::npos) {
    parts.push_back(str.substr(start, separator - start));
    start = separator + delimiter.size();
    separator = str.find(delimiter, start);
  }
  parts.push_back(str.substr(start));
  arr->insert(arr->end(), std::make_move_iterator(parts.begin()), std::make_move_iterator(parts.end()));
}

void stringTok(const std::string& str, std::string_view delimiters, std::vector<std::string> * tokens)
{
  if (tokens == nullptr) {
    throw Exception("Missing token output");
  }
  std::vector<std::string> nextTokens;

  // Skip delimiters at beginning.
  std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);

  // Find first "non-delimiter".
  std::string::size_type pos = str.find_first_of(delimiters, lastPos);

  while (std::string::npos != pos || std::string::npos != lastPos) {
    // Found a token, add it to the vector.
    nextTokens.push_back(str.substr(lastPos, pos - lastPos));
    // Skip delimiters.  Note the "not_of"
    lastPos = str.find_first_not_of(delimiters, pos);
    // Find next "non-delimiter"
    pos = str.find_first_of(delimiters, lastPos);
  }
  tokens->insert(tokens->end(), std::make_move_iterator(nextTokens.begin()), std::make_move_iterator(nextTokens.end()));
}

} // namespace mustache
