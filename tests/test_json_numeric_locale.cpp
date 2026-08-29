#include "mustache_config.h"

#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "mustache.hpp"

namespace {

int failures = 0;
constexpr int skipped = 77;

void expect(bool condition, const char * message)
{
  if (!condition) {
    std::fprintf(stderr, "%s\n", message);
    ++failures;
  }
}

void expectEqual(const char * label, const std::string& actual, const std::string& expected)
{
  if (actual != expected) {
    std::fprintf(
        stderr, "%s failed\n  expected: \"%s\"\n  actual:   \"%s\"\n", label, expected.c_str(), actual.c_str());
    ++failures;
  }
}

bool usesCommaDecimalPoint()
{
  const std::lconv * conventions = std::localeconv();
  return conventions != NULL && conventions->decimal_point != NULL && std::strcmp(conventions->decimal_point, ",") == 0;
}

} // namespace

int main()
{
  const char * currentLocale = std::setlocale(LC_NUMERIC, NULL);
  if (currentLocale == NULL) {
    std::fprintf(stderr, "could not read the current numeric locale\n");
    return 1;
  }
  const std::string savedLocale(currentLocale);

  const char * commaLocales[] = {
      "de_DE.UTF-8", "de_DE.utf8", "de_DE", "fr_FR.UTF-8", "fr_FR.utf8", "fr_FR", "de-DE", "fr-FR"};
  bool commaLocaleAvailable = false;
  int result = 0;
#if defined(_MSC_VER)
  char * requestedLocaleBuffer = nullptr;
  std::size_t requestedLocaleSize = 0;
  if (_dupenv_s(&requestedLocaleBuffer, &requestedLocaleSize, "MUSTACHE_TEST_NUMERIC_LOCALE") != 0) {
    std::fprintf(stderr, "could not read MUSTACHE_TEST_NUMERIC_LOCALE\n");
    return 1;
  }
  const std::unique_ptr<char, decltype(&std::free)> ownedRequestedLocale(requestedLocaleBuffer, &std::free);
  const char * requestedLocale = ownedRequestedLocale.get();
#else
  const char * requestedLocale = std::getenv("MUSTACHE_TEST_NUMERIC_LOCALE");
#endif
  if (requestedLocale != NULL) {
    if (requestedLocale[0] == '\0') {
      std::fprintf(stderr, "MUSTACHE_TEST_NUMERIC_LOCALE must not be empty\n");
      result = 1;
    } else if (std::setlocale(LC_NUMERIC, requestedLocale) == NULL) {
      std::fprintf(stderr, "required numeric locale '%s' is unavailable\n", requestedLocale);
      result = 1;
    } else if (!usesCommaDecimalPoint()) {
      std::fprintf(stderr, "required numeric locale '%s' does not use a comma decimal separator\n", requestedLocale);
      result = 1;
    } else {
      commaLocaleAvailable = true;
    }
  } else {
    for (const char * locale : commaLocales) {
      if (std::setlocale(LC_NUMERIC, locale) != NULL && usesCommaDecimalPoint()) {
        commaLocaleAvailable = true;
        break;
      }
    }
  }

  if (result == 0 && !commaLocaleAvailable) {
    std::fprintf(stderr,
        "SKIP: no comma-decimal numeric locale is available; set MUSTACHE_TEST_NUMERIC_LOCALE to require one\n");
    result = skipped;
  } else if (result == 0) {
    try {
      mustache::Data data = mustache::Data::fromJSON("{\"decimal\":1.50,\"exponent\":1.50e2}");
      const mustache::Data * decimal = data.find("decimal");
      const mustache::Data * exponent = data.find("exponent");
      expect(decimal != NULL && exponent != NULL, "locale test JSON members were not parsed");
      if (decimal != NULL && exponent != NULL) {
        expectEqual("locale-independent JSON decimal spelling", decimal->toString(), "1.50");
        expectEqual("locale-independent JSON exponent spelling", exponent->toString(), "1.50e2");
        expect(decimal->floatingValue() == 1.5 && exponent->floatingValue() == 150.0,
            "numeric locale changed parsed JSON floating-point values");
      }
    } catch (const mustache::Exception& exception) {
      std::fprintf(stderr, "locale-independent JSON parsing failed: %s\n", exception.what());
      ++failures;
    }
  }

  if (std::setlocale(LC_NUMERIC, savedLocale.c_str()) == NULL) {
    std::fprintf(stderr, "could not restore the numeric locale\n");
    result = 1;
  }

  if (failures != 0) {
    result = 1;
  }
  return result;
}
