#include "data.hpp"

#include <cmath>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

// nlohmann/json 3.10 does not support namespace customization. The CMake and
// Autotools rules compile this complete translation unit with hidden symbol
// visibility so minimum-version builds cannot export or interpose dependency
// template instantiations. Newer releases additionally receive a
// libmustache-private ABI namespace.
#define NLOHMANN_JSON_NAMESPACE nlohmann::mustache_private_json
#define NLOHMANN_JSON_NAMESPACE_BEGIN                                                                                  \
  namespace nlohmann {                                                                                                 \
  inline namespace mustache_private_json {
#define NLOHMANN_JSON_NAMESPACE_END                                                                                    \
  }                                                                                                                    \
  }
#include <nlohmann/json.hpp>
#undef NLOHMANN_JSON_NAMESPACE_END
#undef NLOHMANN_JSON_NAMESPACE_BEGIN
#undef NLOHMANN_JSON_NAMESPACE

namespace mustache {

namespace {

const std::size_t parseNestingCeiling = 256;

std::optional<std::string> normalizeFloatingSpelling(const std::string& spelling)
{
  std::string normalized = spelling;
  std::size_t position = 0;
  if (position < normalized.size() && normalized[position] == '-') {
    ++position;
  }

  const std::size_t integerStart = position;
  while (position < normalized.size() && normalized[position] >= '0' && normalized[position] <= '9') {
    ++position;
  }
  if (position == integerStart) {
    return std::nullopt;
  }

  bool floatingPoint = false;
  if (position < normalized.size() && normalized[position] != 'e' && normalized[position] != 'E') {
    // nlohmann/json through 3.11.3 replaces the JSON decimal point in the
    // SAX spelling with the current C locale's one-byte decimal separator.
    // Reconstruct the locale-independent JSON token from its validated shape.
    normalized[position++] = '.';
    const std::size_t fractionStart = position;
    while (position < normalized.size() && normalized[position] >= '0' && normalized[position] <= '9') {
      ++position;
    }
    if (position == fractionStart) {
      return std::nullopt;
    }
    floatingPoint = true;
  }

  if (position < normalized.size() && (normalized[position] == 'e' || normalized[position] == 'E')) {
    ++position;
    if (position < normalized.size() && (normalized[position] == '+' || normalized[position] == '-')) {
      ++position;
    }
    const std::size_t exponentStart = position;
    while (position < normalized.size() && normalized[position] >= '0' && normalized[position] <= '9') {
      ++position;
    }
    if (position == exponentStart) {
      return std::nullopt;
    }
    floatingPoint = true;
  }

  if (position != normalized.size() || !floatingPoint) {
    return std::nullopt;
  }
  return normalized;
}

class JSONParseBudget {
  public:
    explicit JSONParseBudget(const Data::ParseLimits& limits) :
        limits_(limits),
        nodes_(0),
        stringBytes_(0),
        containerEntries_(0)
    {}

    void addNode(std::size_t depth)
    {
      if (depth >= limits_.maxNestingDepth || depth >= parseNestingCeiling) {
        fail(" nesting limit exceeded");
      }
      consume(nodes_, 1, limits_.maxNodes, " node count limit exceeded");
    }

    void addString(std::size_t bytes)
    {
      consume(stringBytes_, bytes, limits_.maxStringBytes, " string byte limit exceeded");
    }

    void addContainerEntries(std::size_t entries)
    {
      consume(containerEntries_, entries, limits_.maxContainerEntries, " container entry limit exceeded");
    }

  private:
    const Data::ParseLimits& limits_;
    std::size_t nodes_;
    std::size_t stringBytes_;
    std::size_t containerEntries_;

    [[noreturn]] void fail(const char * description) const
    {
      throw Exception(std::string("JSON") + description);
    }

    void consume(std::size_t& used, std::size_t amount, std::size_t maximum, const char * description)
    {
      if (used > maximum || amount > maximum - used) {
        fail(description);
      }
      used += amount;
    }
};

} // namespace

#if defined(__GNUC__) && !defined(_WIN32)
#define MUSTACHE_LOCAL_CLASS __attribute__((visibility("hidden")))
#else
#define MUSTACHE_LOCAL_CLASS
#endif

class MUSTACHE_LOCAL_CLASS Data::JSONDataBuilder final : public nlohmann::json_sax<nlohmann::json> {
  public:
    explicit JSONDataBuilder(const Data::ParseLimits& limits) :
        budget_(limits)
    {}

    bool null() override
    {
      addScalar(Data::null());
      return true;
    }

    bool boolean(bool value) override
    {
      addScalar(Data::boolean(value));
      return true;
    }

    bool number_integer(number_integer_t value) override
    {
      addScalar(Data::integer(value));
      return true;
    }

    bool number_unsigned(number_unsigned_t value) override
    {
      beginValue();
      if (value > static_cast<number_unsigned_t>(std::numeric_limits<std::int64_t>::max())) {
        throw Exception("JSON integer is outside the supported range");
      }
      addValue(Data::integer(static_cast<std::int64_t>(value)));
      return true;
    }

    bool number_float(number_float_t value, const string_t& spelling) override
    {
      beginValue();
      std::optional<std::string> normalized = normalizeFloatingSpelling(spelling);
      if (!normalized.has_value()) {
        throw Exception("JSON integer is outside the supported range");
      }
      if (!std::isfinite(value)) {
        throw Exception("Invalid JSON floating-point value");
      }

      budget_.addString(normalized->size());
      addValue(Data::parsedFloating(value, std::move(*normalized)));
      return true;
    }

    bool string(string_t& value) override
    {
      beginValue();
      budget_.addString(value.size());
      addValue(Data::string(std::move(value)));
      return true;
    }

    bool binary(binary_t&) override
    {
      return false;
    }

    bool start_object(std::size_t) override
    {
      beginValue();
      frames_.emplace_back(Data::object());
      return true;
    }

    bool key(string_t& value) override
    {
      if (frames_.empty() || frames_.back().value.type() != Data::TypeMap || frames_.back().key.has_value()) {
        return false;
      }
      budget_.addContainerEntries(1);
      budget_.addString(value.size());
      frames_.back().key = std::move(value);
      return true;
    }

    bool end_object() override
    {
      return endContainer(Data::TypeMap);
    }

    bool start_array(std::size_t) override
    {
      beginValue();
      frames_.emplace_back(Data::array());
      return true;
    }

    bool end_array() override
    {
      return endContainer(Data::TypeArray);
    }

    bool parse_error(std::size_t, const std::string&, const nlohmann::detail::exception&) override
    {
      return false;
    }

    Data takeResult()
    {
      if (!result_.has_value() || !frames_.empty()) {
        throw Exception("Invalid JSON data");
      }
      return std::move(*result_);
    }

  private:
    struct Frame {
        explicit Frame(Data value) :
            value(std::move(value))
        {}

        Data value;
        std::optional<std::string> key;
    };

    JSONParseBudget budget_;
    std::vector<Frame> frames_;
    std::optional<Data> result_;

    void addScalar(Data value)
    {
      beginValue();
      addValue(std::move(value));
    }

    void beginValue()
    {
      if (!frames_.empty() && frames_.back().value.type() == Data::TypeArray) {
        budget_.addContainerEntries(1);
      }
      budget_.addNode(frames_.size());
    }

    void addValue(Data value)
    {
      if (frames_.empty()) {
        if (result_.has_value()) {
          throw Exception("Invalid JSON data");
        }
        result_.emplace(std::move(value));
        return;
      }

      Frame& parent = frames_.back();
      if (parent.value.type() == Data::TypeArray) {
        parent.value.push_back(std::move(value));
        return;
      }
      if (parent.value.type() != Data::TypeMap || !parent.key.has_value()) {
        throw Exception("Invalid JSON data");
      }
      parent.value.set(std::move(*parent.key), std::move(value));
      parent.key.reset();
    }

    bool endContainer(Data::Type type)
    {
      if (frames_.empty() || frames_.back().value.type() != type || frames_.back().key.has_value()) {
        return false;
      }

      Data value = std::move(frames_.back().value);
      frames_.pop_back();
      addValue(std::move(value));
      return true;
    }
};

#undef MUSTACHE_LOCAL_CLASS

Data Data::parseJSON(std::string_view string, const Data::ParseLimits& limits)
{
  JSONDataBuilder builder(limits);
  bool parsed = false;
  try {
    parsed = nlohmann::json::sax_parse(
        string.begin(), string.end(), &builder, nlohmann::json::input_format_t::json, true, false);
  } catch (const nlohmann::json::exception&) {
    throw Exception("Invalid JSON data");
  }
  if (!parsed) {
    throw Exception("Invalid JSON data");
  }
  return builder.takeResult();
}

} // namespace mustache
