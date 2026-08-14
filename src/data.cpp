#include "data.hpp"

#include "yaml.h"

#include "json.h"
#include "json_c_version.h"
#include "json_object.h"
#include "json_tokener.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace mustache {

struct Data::Storage {
  typedef std::variant<std::monostate, String, List, Map, Array,
      std::shared_ptr<Lambda>, bool, std::int64_t, double> Value;

  Value value;
  std::string scalarSpelling;

  Storage() = default;
  Storage(const Storage&) = default;
  Storage(Storage&&) noexcept = default;

  template <typename T, typename = std::enable_if_t<
      !std::is_same_v<std::decay_t<T>, Storage> > >
  explicit Storage(T&& value) : value(std::forward<T>(value)) {}
};

std::unique_ptr<Data::Storage> Data::makeStorage(Type type, int size)
{
  if( size < 0 ) {
    throw Exception("Invalid data size");
  }

  switch( type ) {
    case Data::TypeNone:
      return std::make_unique<Storage>();
    case Data::TypeString: {
      Data::String value;
      value.reserve(static_cast<std::size_t>(size));
      return std::make_unique<Storage>(std::move(value));
    }
    case Data::TypeList:
      return std::make_unique<Storage>(List());
    case Data::TypeMap: {
      Data::Map value;
      value.reserve(static_cast<std::size_t>(size));
      return std::make_unique<Storage>(std::move(value));
    }
    case Data::TypeArray: {
      Data::Array value;
      value.reserve(static_cast<std::size_t>(size));
      return std::make_unique<Storage>(std::move(value));
    }
    case Data::TypeLambda:
      return std::make_unique<Storage>(std::shared_ptr<Lambda>());
    case Data::TypeBoolean:
      return std::make_unique<Storage>(false);
    case Data::TypeInteger:
      return std::make_unique<Storage>(std::int64_t(0));
    case Data::TypeDouble:
      return std::make_unique<Storage>(0.0);
  }

  throw Exception("Unknown data type");
}

namespace {

const char * typeDescription(Data::Type type)
{
  switch( type ) {
    case Data::TypeNone:
      return "null";
    case Data::TypeString:
      return "string";
    case Data::TypeList:
      return "list";
    case Data::TypeMap:
      return "object";
    case Data::TypeArray:
      return "array";
    case Data::TypeLambda:
      return "lambda";
    case Data::TypeBoolean:
      return "boolean";
    case Data::TypeInteger:
      return "integer";
    case Data::TypeDouble:
      return "floating-point";
  }
  return "unknown";
}

} // namespace

Data::ParseLimits::ParseLimits() :
    maxInputBytes(64 * 1024 * 1024),
    maxNestingDepth(32),
    maxNodes(100000),
    maxStringBytes(64 * 1024 * 1024),
    maxContainerEntries(100000)
{
}

Data::Data() : storage_(std::make_unique<Storage>()) {}

Data::Data(Type type, int size) : storage_(makeStorage(type, size)) {}

Data::Data(std::unique_ptr<Storage> storage) noexcept :
    storage_(std::move(storage)) {}

Data::Data(const Data& other) :
    storage_(other.storage_ == nullptr
        ? std::make_unique<Storage>()
        : std::make_unique<Storage>(*other.storage_)) {}

Data& Data::operator=(const Data& other)
{
  if( this != &other ) {
    Data copy(other);
    swap(copy);
  }
  return *this;
}

Data::Data(Data&& other) noexcept = default;

Data& Data::operator=(Data&& other) noexcept = default;

Data::~Data() = default;

Data Data::null()
{
  return Data();
}

Data Data::boolean(bool value)
{
  return Data(std::make_unique<Storage>(value));
}

Data Data::integer(std::int64_t value)
{
  return Data(std::make_unique<Storage>(value));
}

Data Data::floating(double value)
{
  if( !std::isfinite(value) ) {
    throw Exception("Invalid floating-point data");
  }
  return Data(std::make_unique<Storage>(value));
}

Data Data::string(std::string value)
{
  return Data(std::make_unique<Storage>(std::move(value)));
}

Data Data::list(List values)
{
  return Data(std::make_unique<Storage>(std::move(values)));
}

Data Data::array(Array values)
{
  return Data(std::make_unique<Storage>(std::move(values)));
}

Data Data::object(Map values)
{
  return Data(std::make_unique<Storage>(std::move(values)));
}

Data Data::lambda(std::unique_ptr<Lambda> value)
{
  return sharedLambda(std::shared_ptr<Lambda>(std::move(value)));
}

Data Data::sharedLambda(std::shared_ptr<Lambda> value)
{
  if( value == nullptr ) {
    throw Exception("Empty lambda data");
  }
  return Data(std::make_unique<Storage>(std::move(value)));
}

void Data::init(Type type, int size)
{
  Data replacement(makeStorage(type, size));
  swap(replacement);
}

Data::Type Data::type() const noexcept
{
  static_assert(std::variant_size<Storage::Value>::value == 9,
      "Update Data::type() when adding a storage alternative");

  if( storage_ == nullptr ) {
    return TypeNone;
  }

  switch( storage_->value.index() ) {
    case 0:
      return TypeNone;
    case 1:
      return TypeString;
    case 2:
      return TypeList;
    case 3:
      return TypeMap;
    case 4:
      return TypeArray;
    case 5:
      return TypeLambda;
    case 6:
      return TypeBoolean;
    case 7:
      return TypeInteger;
    case 8:
      return TypeDouble;
  }

  return TypeNone;
}

int Data::isEmpty() const noexcept
{
  switch( type() ) {
    case TypeNone:
      return 1;
    case TypeString:
      return std::get<String>(storage_->value).empty() ? 1 : 0;
    case TypeList:
      return std::get<List>(storage_->value).empty() ? 1 : 0;
    case TypeMap:
      return std::get<Map>(storage_->value).empty() ? 1 : 0;
    case TypeArray:
      return std::get<Array>(storage_->value).empty() ? 1 : 0;
    case TypeLambda:
      return std::get<std::shared_ptr<Lambda> >(storage_->value) == nullptr
          ? 1 : 0;
    case TypeBoolean:
      return std::get<bool>(storage_->value) ? 0 : 1;
    case TypeInteger:
    case TypeDouble:
      return 0;
  }
  return 1;
}

Data::Storage& Data::requireStorage(Type expected, const char * description)
{
  if( type() != expected ) {
    throw Exception(std::string("Data is not ") + description);
  }
  return *storage_;
}

const Data::Storage& Data::requireStorage(
    Type expected, const char * description) const
{
  if( type() != expected ) {
    throw Exception(std::string("Data is not ") + description);
  }
  return *storage_;
}

Data::String& Data::stringValue()
{
  return std::get<String>(requireStorage(TypeString, "a string").value);
}

const Data::String& Data::stringValue() const
{
  return std::get<String>(requireStorage(TypeString, "a string").value);
}

bool Data::booleanValue() const
{
  return std::get<bool>(requireStorage(TypeBoolean, "a boolean").value);
}

std::int64_t Data::integerValue() const
{
  return std::get<std::int64_t>(
      requireStorage(TypeInteger, "an integer").value);
}

double Data::floatingValue() const
{
  return std::get<double>(
      requireStorage(TypeDouble, "a floating-point number").value);
}

const Data::List& Data::listItems() const
{
  return std::get<List>(requireStorage(TypeList, "a list").value);
}

const Data::Array& Data::arrayItems() const
{
  return std::get<Array>(requireStorage(TypeArray, "an array").value);
}

const Data::Map& Data::objectItems() const
{
  return std::get<Map>(requireStorage(TypeMap, "an object").value);
}

Lambda * Data::lambdaValue() const noexcept
{
  if( type() != TypeLambda ) {
    return nullptr;
  }
  return std::get<std::shared_ptr<Lambda> >(storage_->value).get();
}

std::string Data::toString() const
{
  switch( type() ) {
    case TypeNone:
      return std::string();
    case TypeString:
      return stringValue();
    case TypeBoolean:
      return booleanValue() ? "true" : std::string();
    case TypeInteger:
      return std::to_string(integerValue());
    case TypeDouble: {
      if( !storage_->scalarSpelling.empty() ) {
        return storage_->scalarSpelling;
      }
      std::ostringstream stream;
      stream.imbue(std::locale::classic());
      stream << std::setprecision(std::numeric_limits<double>::max_digits10)
             << floatingValue();
      return stream.str();
    }
    case TypeList:
    case TypeMap:
    case TypeArray:
    case TypeLambda:
      throw Exception(std::string("Cannot render ") +
          typeDescription(type()) + " data as a scalar");
  }
  throw Exception("Unknown data type");
}

Data& Data::set(std::string key, Data value)
{
  Map& values = std::get<Map>(requireStorage(TypeMap, "an object").value);
  values.insert_or_assign(std::move(key), std::move(value));
  return *this;
}

Data& Data::push_back(Data value)
{
  if( type() == TypeList ) {
    std::get<List>(storage_->value).push_back(std::move(value));
  } else if( type() == TypeArray ) {
    std::get<Array>(storage_->value).push_back(std::move(value));
  } else {
    throw Exception("Data is not a list or array");
  }
  return *this;
}

const Data * Data::find(const std::string& key) const noexcept
{
  if( type() != TypeMap ) {
    return nullptr;
  }
  const Map& values = std::get<Map>(storage_->value);
  const Map::const_iterator found = values.find(key);
  return found == values.end() ? nullptr : &found->second;
}

void Data::swap(Data& other) noexcept
{
  storage_.swap(other.storage_);
}

// Data integrations

namespace {

const std::size_t parseNestingCeiling = 256;

class ParseBudget {
  public:
    ParseBudget(const Data::ParseLimits& limits, const char * format) :
        limits_(limits), format_(format), nodes_(0), stringBytes_(0),
        containerEntries_(0) {}

    void addNode(std::size_t depth)
    {
      if( depth >= limits_.maxNestingDepth ||
          depth >= parseNestingCeiling ) {
        fail(" nesting limit exceeded");
      }
      consume(nodes_, 1, limits_.maxNodes, " node count limit exceeded");
    }

    void addString(std::size_t bytes)
    {
      consume(stringBytes_, bytes, limits_.maxStringBytes,
          " string byte limit exceeded");
    }

    void addContainerEntries(std::size_t entries)
    {
      consume(containerEntries_, entries, limits_.maxContainerEntries,
          " container entry limit exceeded");
    }

  private:
    const Data::ParseLimits& limits_;
    const char * format_;
    std::size_t nodes_;
    std::size_t stringBytes_;
    std::size_t containerEntries_;

    [[noreturn]] void fail(const char * description) const
    {
      throw Exception(std::string(format_) + description);
    }

    void consume(std::size_t& used, std::size_t amount,
        std::size_t maximum, const char * description)
    {
      if( used > maximum || amount > maximum - used ) {
        fail(description);
      }
      used += amount;
    }
};

void checkInputSize(std::string_view input,
    const Data::ParseLimits& limits, const char * format)
{
  if( input.size() > limits.maxInputBytes ) {
    throw Exception(std::string(format) + " input byte limit exceeded");
  }
  if( input.find('\0') != std::string_view::npos ) {
    throw Exception(std::string(format) + " input contains NUL byte");
  }
}

int jsonHexDigit(char value)
{
  if( value >= '0' && value <= '9' ) {
    return value - '0';
  }
  if( value >= 'a' && value <= 'f' ) {
    return value - 'a' + 10;
  }
  if( value >= 'A' && value <= 'F' ) {
    return value - 'A' + 10;
  }
  return -1;
}

class JSONPreflight {
  public:
    JSONPreflight(
        std::string_view input, const Data::ParseLimits& limits) :
        input_(input), position_(0), budget_(limits, "JSON") {}

    void run()
    {
      skipWhitespace();
      if( !parseValue(0) ) {
        throw Exception("Invalid JSON data");
      }
      skipWhitespace();
      if( position_ != input_.size() ) {
        throw Exception("Invalid JSON data");
      }
    }

  private:
    std::string_view input_;
    std::size_t position_;
    ParseBudget budget_;

    void skipWhitespace()
    {
      while( position_ < input_.size() ) {
        const char value = input_[position_];
        if( value != ' ' && value != '\t' &&
            value != '\r' && value != '\n' ) {
          return;
        }
        ++position_;
      }
    }

    bool parseValue(std::size_t depth)
    {
      skipWhitespace();
      if( position_ >= input_.size() ) {
        return false;
      }

      const char value = input_[position_];
      if( value == '{' ) {
        budget_.addNode(depth);
        return parseObject(depth);
      }
      if( value == '[' ) {
        budget_.addNode(depth);
        return parseArray(depth);
      }
      if( value == '"' ) {
        budget_.addNode(depth);
        return parseString(false);
      }
      if( value == ']' || value == '}' || value == ',' || value == ':' ) {
        return false;
      }

      budget_.addNode(depth);
      if( value == 'n' ) {
        return parseLiteral("null");
      }
      if( value == 't' ) {
        return parseLiteral("true");
      }
      if( value == 'f' ) {
        return parseLiteral("false");
      }
      return parseNumber();
    }

    bool parseObject(std::size_t depth)
    {
      ++position_;
      skipWhitespace();
      if( consume('}') ) {
        return true;
      }

      while( position_ < input_.size() ) {
        if( input_[position_] != '"' ) {
          return false;
        }
        budget_.addContainerEntries(1);
        if( !parseString(true) ) {
          return false;
        }
        skipWhitespace();
        if( !consume(':') || !parseValue(depth + 1) ) {
          return false;
        }
        skipWhitespace();
        if( consume('}') ) {
          return true;
        }
        if( !consume(',') ) {
          return false;
        }
        skipWhitespace();
      }
      return false;
    }

    bool parseArray(std::size_t depth)
    {
      ++position_;
      skipWhitespace();
      if( consume(']') ) {
        return true;
      }

      while( position_ < input_.size() ) {
        budget_.addContainerEntries(1);
        if( !parseValue(depth + 1) ) {
          return false;
        }
        skipWhitespace();
        if( consume(']') ) {
          return true;
        }
        if( !consume(',') ) {
          return false;
        }
        skipWhitespace();
      }
      return false;
    }

    bool parseString(bool objectKey)
    {
      ++position_;
      std::size_t decodedBytes = 0;
      bool containsNul = false;

      while( position_ < input_.size() ) {
        const unsigned char value =
            static_cast<unsigned char>(input_[position_++]);
        if( value == '"' ) {
          if( objectKey && containsNul ) {
            throw Exception("JSON object keys may not contain NUL");
          }
          budget_.addString(decodedBytes);
          return true;
        }
        if( value < 0x20 ) {
          return false;
        }
        if( value != '\\' ) {
          ++decodedBytes;
          continue;
        }
        if( position_ >= input_.size() ) {
          return false;
        }

        const char escape = input_[position_++];
        if( escape != 'u' ) {
          if( escape != '"' && escape != '\\' && escape != '/' &&
              escape != 'b' && escape != 'f' && escape != 'n' &&
              escape != 'r' && escape != 't' ) {
            return false;
          }
          ++decodedBytes;
          continue;
        }

        unsigned int codeUnit = 0;
        if( !parseCodeUnit(codeUnit) ) {
          return false;
        }
        if( codeUnit == 0 ) {
          containsNul = true;
        }

        if( codeUnit >= 0xd800 && codeUnit <= 0xdbff &&
            position_ + 6 <= input_.size() &&
            input_[position_] == '\\' && input_[position_ + 1] == 'u' ) {
          const std::size_t lowStart = position_;
          position_ += 2;
          unsigned int lowCodeUnit = 0;
          if( parseCodeUnit(lowCodeUnit) &&
              lowCodeUnit >= 0xdc00 && lowCodeUnit <= 0xdfff ) {
            decodedBytes += 4;
            continue;
          }
          position_ = lowStart;
        }

        if( codeUnit <= 0x7f ) {
          ++decodedBytes;
        } else if( codeUnit <= 0x7ff ) {
          decodedBytes += 2;
        } else {
          decodedBytes += 3;
        }
      }
      return false;
    }

    bool parseLiteral(std::string_view literal)
    {
      if( input_.substr(position_, literal.size()) != literal ) {
        return false;
      }
      position_ += literal.size();
      return true;
    }

    bool parseNumber()
    {
      const std::size_t start = position_;
      if( consume('-') && position_ >= input_.size() ) {
        return false;
      }

      if( consume('0') ) {
        if( position_ < input_.size() && isDigit(input_[position_]) ) {
          return false;
        }
      } else {
        if( position_ >= input_.size() ||
            input_[position_] < '1' || input_[position_] > '9' ) {
          return false;
        }
        do {
          ++position_;
        } while( position_ < input_.size() && isDigit(input_[position_]) );
      }

      bool floatingPoint = false;
      if( consume('.') ) {
        floatingPoint = true;
        if( position_ >= input_.size() || !isDigit(input_[position_]) ) {
          return false;
        }
        do {
          ++position_;
        } while( position_ < input_.size() && isDigit(input_[position_]) );
      }

      if( position_ < input_.size() &&
          (input_[position_] == 'e' || input_[position_] == 'E') ) {
        floatingPoint = true;
        ++position_;
        if( position_ < input_.size() &&
            (input_[position_] == '+' || input_[position_] == '-') ) {
          ++position_;
        }
        if( position_ >= input_.size() || !isDigit(input_[position_]) ) {
          return false;
        }
        do {
          ++position_;
        } while( position_ < input_.size() && isDigit(input_[position_]) );
      }

      if( floatingPoint ) {
        budget_.addString(position_ - start);
      }
      return true;
    }

    static bool isDigit(char value)
    {
      return value >= '0' && value <= '9';
    }

    bool parseCodeUnit(unsigned int& codeUnit)
    {
      if( position_ + 4 > input_.size() ) {
        return false;
      }
      codeUnit = 0;
      for( std::size_t i = 0; i < 4; ++i ) {
        const int digit = jsonHexDigit(input_[position_ + i]);
        if( digit < 0 ) {
          return false;
        }
        codeUnit = (codeUnit << 4) | static_cast<unsigned int>(digit);
      }
      position_ += 4;
      return true;
    }

    bool consume(char expected)
    {
      if( position_ >= input_.size() || input_[position_] != expected ) {
        return false;
      }
      ++position_;
      return true;
    }
};

void preflightJSON(
    std::string_view input, const Data::ParseLimits& limits)
{
  // json-c does not expose object-key lengths through its object iterator.
  // Validate and budget the source before building its DOM so escaped NULs
  // cannot collapse distinct keys and large documents cannot bypass limits
  // until after allocation. Conversion below independently rechecks the
  // produced value tree.
  JSONPreflight(input, limits).run();
}

} // namespace

Data Data::fromJSON(const char * string)
{
  return fromJSON(string, ParseLimits());
}

Data Data::fromJSON(const char * string, const ParseLimits& limits)
{
  if( string == nullptr ) {
    throw Exception("Missing JSON data");
  }
  return fromJSON(std::string_view(string), limits);
}

Data Data::fromJSON(std::string_view string)
{
  return fromJSON(string, ParseLimits());
}

Data * Data::createFromJSON(const char * string)
{
  return new Data(fromJSON(string));
}

Data * Data::createFromJSON(
    const char * string, const ParseLimits& limits)
{
  return new Data(fromJSON(string, limits));
}

Data Data::fromJSON(std::string_view string, const ParseLimits& limits)
{
  checkInputSize(string, limits, "JSON");
  preflightJSON(string, limits);
  if( string.size() >= static_cast<std::size_t>(
          std::numeric_limits<int>::max()) ) {
    throw Exception("JSON input is too large");
  }

  const std::size_t configuredDepth =
      std::min(limits.maxNestingDepth, parseNestingCeiling);
  const int parserDepth = static_cast<int>(
      std::min(configuredDepth + 1, parseNestingCeiling));
  std::unique_ptr<json_tokener, decltype(&json_tokener_free)> tokener(
      json_tokener_new_ex(parserDepth), &json_tokener_free);
  if( tokener == nullptr ) {
    throw Exception("Failed to initialize JSON parser");
  }
  json_tokener_set_flags(tokener.get(), JSON_TOKENER_STRICT);

  // json-c needs an explicit terminator to complete top-level numbers. The
  // owned copy also lets the string-view overload safely accept bounded input.
  std::string terminated;
  terminated.reserve(string.size() + 1);
  if( !string.empty() ) {
    terminated.assign(string.data(), string.size());
  }
  terminated.push_back('\0');
  std::unique_ptr<json_object, decltype(&json_object_put)> result(
      json_tokener_parse_ex(tokener.get(), terminated.data(),
          static_cast<int>(terminated.size())),
      &json_object_put);
  const json_tokener_error parseError = json_tokener_get_error(tokener.get());
  if( parseError == json_tokener_error_depth ) {
    throw Exception("JSON nesting limit exceeded");
  }
#if JSON_C_VERSION_NUM >= 0x000e00
  const std::size_t parseEnd = json_tokener_get_parse_end(tokener.get());
#else
  const std::size_t parseEnd = static_cast<std::size_t>(tokener->char_offset);
#endif
  if( parseError != json_tokener_success || parseEnd != string.size() ) {
    throw Exception("Invalid JSON data");
  }

  ParseBudget budget(limits, "JSON");
  if( result == nullptr ) {
    budget.addNode(0);
    return Data::null();
  }

  const auto convert = [&budget](const auto& self, json_object * object,
                           std::size_t depth) -> Data {
    budget.addNode(depth);
    if( object == nullptr ) {
      return Data::null();
    }

    switch( json_object_get_type(object) ) {
      case json_type_null:
        return Data::null();
      case json_type_boolean:
        return Data::boolean(json_object_get_boolean(object) != 0);
      case json_type_double: {
        const char * spelling = json_object_get_string(object);
        if( spelling == nullptr ) {
          throw Exception("Invalid JSON floating-point value");
        }
        budget.addString(std::char_traits<char>::length(spelling));
        Data value(std::make_unique<Storage>(
            json_object_get_double(object)));
        value.storage_->scalarSpelling = spelling;
        return value;
      }
      case json_type_int: {
        const std::int64_t value = json_object_get_int64(object);
        if( std::to_string(value) != json_object_get_string(object) ) {
          throw Exception("JSON integer is outside the supported range");
        }
        return Data::integer(value);
      }
      case json_type_string: {
        const std::size_t length = static_cast<std::size_t>(
            json_object_get_string_len(object));
        budget.addString(length);
        if( length == 0 ) {
          return Data::string(std::string());
        }
        return Data::string(std::string(json_object_get_string(object), length));
      }
      case json_type_object: {
        const std::size_t childCount = static_cast<std::size_t>(
            json_object_object_length(object));
        budget.addContainerEntries(childCount);
        if( childCount > static_cast<std::size_t>(
                std::numeric_limits<int>::max()) ) {
          throw Exception("JSON object is too large");
        }
        Data value(Data::TypeMap, static_cast<int>(childCount));
        json_object_object_foreach(object, key, child)
        {
          // The preflight pass rejects escaped NULs before json-c can
          // truncate an object key, so this C-string API is safe here.
          const std::size_t keyLength = std::char_traits<char>::length(key);
          budget.addString(keyLength);
          value.set(std::string(key, keyLength),
              self(self, child, depth + 1));
        }
        return value;
      }
      case json_type_array: {
        const std::size_t childCount = json_object_array_length(object);
        budget.addContainerEntries(childCount);
        if( childCount > static_cast<std::size_t>(
                std::numeric_limits<int>::max()) ) {
          throw Exception("JSON array is too large");
        }
        Data value(Data::TypeArray, static_cast<int>(childCount));
        for( std::size_t i = 0; i < childCount; ++i ) {
          value.push_back(self(
              self, json_object_array_get_idx(object, i), depth + 1));
        }
        return value;
      }
    }

    throw Exception("Unknown json type");
  };

  return convert(convert, result.get(), 0);
}

Data Data::fromYAML(const char * string)
{
  return fromYAML(string, ParseLimits());
}

Data Data::fromYAML(const char * string, const ParseLimits& limits)
{
  if( string == nullptr ) {
    throw Exception("Missing YAML data");
  }
  return fromYAML(std::string_view(string), limits);
}

Data Data::fromYAML(std::string_view string)
{
  return fromYAML(string, ParseLimits());
}

Data * Data::createFromYAML(const char * string)
{
  return new Data(fromYAML(string));
}

Data * Data::createFromYAML(
    const char * string, const ParseLimits& limits)
{
  return new Data(fromYAML(string, limits));
}

namespace {

class YAMLPreflight {
  public:
    YAMLPreflight(
        std::string_view input, const Data::ParseLimits& limits) :
        input_(input), budget_(limits, "YAML"), sawRoot_(false),
        completedDocuments_(0) {}

    void run()
    {
      yaml_parser_t parser;
      if( yaml_parser_initialize(&parser) == 0 ) {
        throw Exception("Failed to initialize yaml parser");
      }

      static const unsigned char emptyInput = 0;
      const unsigned char * input = input_.empty()
          ? &emptyInput
          : reinterpret_cast<const unsigned char *>(input_.data());
      yaml_parser_set_input_string(&parser, input, input_.size());

      try {
        bool finished = false;
        while( !finished ) {
          yaml_event_t event;
          if( yaml_parser_parse(&parser, &event) == 0 ) {
            if( completedDocuments_ != 0 ) {
              throw Exception("Invalid trailing YAML content");
            }
            throw Exception("Failed to parse yaml document");
          }

          try {
            finished = handle(event);
          } catch( ... ) {
            yaml_event_delete(&event);
            throw;
          }
          yaml_event_delete(&event);
        }
      } catch( ... ) {
        yaml_parser_delete(&parser);
        throw;
      }
      yaml_parser_delete(&parser);

      if( !sawRoot_ ) {
        throw Exception("Empty yaml document");
      }
    }

  private:
    enum class ContainerType {
      Sequence,
      Mapping
    };

    struct Container {
      ContainerType type;
      bool expectingKey;
    };

    std::string_view input_;
    ParseBudget budget_;
    std::vector<Container> containers_;
    bool sawRoot_;
    std::size_t completedDocuments_;

    bool handle(const yaml_event_t& event)
    {
      switch( event.type ) {
        case YAML_NO_EVENT:
        case YAML_STREAM_START_EVENT:
          return false;
        case YAML_STREAM_END_EVENT:
          return true;
        case YAML_DOCUMENT_START_EVENT:
          if( completedDocuments_ != 0 ) {
            throw Exception("Multiple YAML documents are not supported");
          }
          return false;
        case YAML_DOCUMENT_END_EVENT:
          if( !containers_.empty() ) {
            throw Exception("Failed to parse yaml document");
          }
          ++completedDocuments_;
          return false;
        case YAML_ALIAS_EVENT:
          consumeAlias();
          return false;
        case YAML_SCALAR_EVENT:
          consumeScalar(event.data.scalar.length);
          return false;
        case YAML_SEQUENCE_START_EVENT:
          startContainer(ContainerType::Sequence);
          return false;
        case YAML_MAPPING_START_EVENT:
          startContainer(ContainerType::Mapping);
          return false;
        case YAML_SEQUENCE_END_EVENT:
          endContainer(ContainerType::Sequence);
          return false;
        case YAML_MAPPING_END_EVENT:
          endContainer(ContainerType::Mapping);
          return false;
      }
      throw Exception("Unknown yaml event");
    }

    bool expectingMappingKey() const
    {
      return !containers_.empty() &&
          containers_.back().type == ContainerType::Mapping &&
          containers_.back().expectingKey;
    }

    void consumeAlias()
    {
      if( expectingMappingKey() ) {
        budget_.addContainerEntries(1);
        containers_.back().expectingKey = false;
        return;
      }
      beginValue();
      finishValue();
    }

    void consumeScalar(std::size_t length)
    {
      if( expectingMappingKey() ) {
        budget_.addContainerEntries(1);
        budget_.addString(length);
        containers_.back().expectingKey = false;
        return;
      }
      beginValue();
      budget_.addString(length);
      finishValue();
    }

    void startContainer(ContainerType type)
    {
      if( expectingMappingKey() ) {
        throw Exception("Invalid yaml object key");
      }
      beginValue();
      containers_.push_back(Container{type, type == ContainerType::Mapping});
    }

    void endContainer(ContainerType expected)
    {
      if( containers_.empty() || containers_.back().type != expected ||
          (expected == ContainerType::Mapping &&
              !containers_.back().expectingKey) ) {
        throw Exception("Failed to parse yaml document");
      }
      containers_.pop_back();
      finishValue();
    }

    void beginValue()
    {
      if( containers_.empty() ) {
        if( sawRoot_ ) {
          throw Exception("Failed to parse yaml document");
        }
        sawRoot_ = true;
      } else if( containers_.back().type == ContainerType::Sequence ) {
        budget_.addContainerEntries(1);
      } else if( containers_.back().expectingKey ) {
        throw Exception("Invalid yaml object key");
      }
      budget_.addNode(containers_.size());
    }

    void finishValue()
    {
      if( !containers_.empty() &&
          containers_.back().type == ContainerType::Mapping &&
          !containers_.back().expectingKey ) {
        containers_.back().expectingKey = true;
      }
    }
};

void preflightYAML(
    std::string_view input, const Data::ParseLimits& limits)
{
  // The event parser avoids libyaml's document-composer behavior on inputs
  // that exceed the public resource policy. The composed/expanded result is
  // checked again below because aliases can multiply the output value tree.
  YAMLPreflight(input, limits).run();
}

class ActiveYAMLNode {
  public:
    ActiveYAMLNode(std::unordered_set<yaml_node_t *>& active,
        yaml_node_t * node) : active_(active), node_(node)
    {
      if( !active_.insert(node_).second ) {
        throw Exception("YAML alias cycle detected");
      }
    }

    ~ActiveYAMLNode()
    {
      active_.erase(node_);
    }

  private:
    std::unordered_set<yaml_node_t *>& active_;
    yaml_node_t * node_;
};

std::string yamlScalarString(const yaml_node_t * node)
{
  if( node->data.scalar.length == 0 ) {
    return std::string();
  }
  return std::string(
      reinterpret_cast<const char *>(node->data.scalar.value),
      node->data.scalar.length);
}

Data createFromYAMLNode(yaml_document_t * document, yaml_node_t * node,
    ParseBudget& budget, std::unordered_set<yaml_node_t *>& active,
    std::size_t depth)
{
  if( node == nullptr ) {
    throw Exception("Missing yaml node");
  }
  budget.addNode(depth);
  ActiveYAMLNode activeNode(active, node);

  switch( node->type ) {
    case YAML_SCALAR_NODE: {
      budget.addString(node->data.scalar.length);
      return Data::string(yamlScalarString(node));
    }
    case YAML_MAPPING_NODE: {
      const std::size_t length = static_cast<std::size_t>(
          node->data.mapping.pairs.top - node->data.mapping.pairs.start);
      budget.addContainerEntries(length);
      if( length > static_cast<std::size_t>(
              std::numeric_limits<int>::max()) ) {
        throw Exception("YAML object is too large");
      }
      Data result(Data::TypeMap, static_cast<int>(length));
      for( yaml_node_pair_t * pair = node->data.mapping.pairs.start;
          pair < node->data.mapping.pairs.top; ++pair ) {
        yaml_node_t * keyNode =
            yaml_document_get_node(document, pair->key);
        yaml_node_t * valueNode =
            yaml_document_get_node(document, pair->value);
        if( keyNode == nullptr || keyNode->type != YAML_SCALAR_NODE ) {
          throw Exception("Invalid yaml object key");
        }
        budget.addString(keyNode->data.scalar.length);
        result.set(yamlScalarString(keyNode), createFromYAMLNode(
            document, valueNode, budget, active, depth + 1));
      }
      return result;
    }
    case YAML_SEQUENCE_NODE: {
      const std::size_t length = static_cast<std::size_t>(
          node->data.sequence.items.top - node->data.sequence.items.start);
      budget.addContainerEntries(length);
      if( length > static_cast<std::size_t>(
              std::numeric_limits<int>::max()) ) {
        throw Exception("YAML array is too large");
      }
      Data result(Data::TypeArray, static_cast<int>(length));
      for( yaml_node_item_t * item = node->data.sequence.items.start;
          item < node->data.sequence.items.top; ++item ) {
        result.push_back(createFromYAMLNode(
            document, yaml_document_get_node(document, *item), budget,
            active, depth + 1));
      }
      return result;
    }
    default:
      throw Exception("Unknown yaml type");
  }
}

} // namespace

Data Data::fromYAML(std::string_view string, const ParseLimits& limits)
{
  checkInputSize(string, limits, "YAML");
  preflightYAML(string, limits);

  yaml_parser_t parser;
  yaml_document_t document;
  if( yaml_parser_initialize(&parser) == 0 ) {
    throw Exception("Failed to initialize yaml parser");
  }

  static const unsigned char emptyInput = 0;
  const unsigned char * input = string.empty()
      ? &emptyInput
      : reinterpret_cast<const unsigned char *>(string.data());
  yaml_parser_set_input_string(&parser, input, string.size());
  if( yaml_parser_load(&parser, &document) == 0 ) {
    yaml_parser_delete(&parser);
    throw Exception("Failed to parse yaml document");
  }

  try {
    yaml_node_t * root = yaml_document_get_root_node(&document);
    if( root == nullptr ) {
      throw Exception("Empty yaml document");
    }
    ParseBudget budget(limits, "YAML");
    std::unordered_set<yaml_node_t *> active;
    Data data = createFromYAMLNode(
        &document, root, budget, active, 0);

    yaml_document_t trailingDocument;
    if( yaml_parser_load(&parser, &trailingDocument) == 0 ) {
      throw Exception("Invalid trailing YAML content");
    }
    const bool hasTrailingDocument =
        yaml_document_get_root_node(&trailingDocument) != nullptr;
    yaml_document_delete(&trailingDocument);
    if( hasTrailingDocument ) {
      throw Exception("Multiple YAML documents are not supported");
    }

    yaml_document_delete(&document);
    yaml_parser_delete(&parser);
    return data;
  } catch( ... ) {
    yaml_document_delete(&document);
    yaml_parser_delete(&parser);
    throw;
  }
}

} // namespace mustache
