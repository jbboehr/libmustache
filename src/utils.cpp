
#include "utils.hpp"

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
  const std::string::size_type strleng = str.length();
  const std::string::size_type delleng = delimiter.length();
  if (delleng == 0) {
    // no change
    return;
  }

  std::string::size_type i = 0;
  std::string::size_type k = 0;
  while (i < strleng) {
    std::string::size_type j = 0;
    while (i + j < strleng && j < delleng && str[i + j] == delimiter[j]) {
      j++;
    }
    if (j == delleng) {
      // found delimiter
      arr->push_back(str.substr(k, i - k));
      i += delleng;
      k = i;
    } else {
      i++;
    }
  }
  arr->push_back(str.substr(k, i - k));
}

void stringTok(const std::string& str, std::string_view delimiters, std::vector<std::string> * tokens)
{
  // Skip delimiters at beginning.
  std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);

  // Find first "non-delimiter".
  std::string::size_type pos = str.find_first_of(delimiters, lastPos);

  while (std::string::npos != pos || std::string::npos != lastPos) {
    // Found a token, add it to the vector.
    tokens->push_back(str.substr(lastPos, pos - lastPos));
    // Skip delimiters.  Note the "not_of"
    lastPos = str.find_first_not_of(delimiters, pos);
    // Find next "non-delimiter"
    pos = str.find_first_of(delimiters, lastPos);
  }
}

} // namespace mustache
