#include "data.hpp"

#ifdef MUSTACHE_HAVE_LIBYAML
#include "yaml.h"
#endif

#if defined(MUSTACHE_HAVE_LIBJSON)
#include "json.h"
#include "json_object.h"
#include "json_tokener.h"
#endif

#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <type_traits>
#include <utility>
#include <variant>

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

#if defined(MUSTACHE_HAVE_LIBJSON)
Data Data::fromJSON(const char * string)
{
  if( string == nullptr ) {
    throw Exception("Missing JSON data");
  }

  const std::size_t length = std::char_traits<char>::length(string);
  if( length >= static_cast<std::size_t>(
          std::numeric_limits<int>::max()) ) {
    throw Exception("JSON input is too large");
  }

  std::unique_ptr<json_tokener, decltype(&json_tokener_free)> tokener(
      json_tokener_new(), &json_tokener_free);
  if( tokener == nullptr ) {
    throw Exception("Failed to initialize JSON parser");
  }
  json_tokener_set_flags(tokener.get(), JSON_TOKENER_STRICT);

  std::unique_ptr<json_object, decltype(&json_object_put)> result(
      json_tokener_parse_ex(tokener.get(), string,
          static_cast<int>(length + 1)),
      &json_object_put);
  if( json_tokener_get_error(tokener.get()) != json_tokener_success ||
      tokener->char_offset != static_cast<int>(length) ) {
    throw Exception("Invalid JSON data");
  }

  if( result == nullptr ) {
    return Data::null();
  }

  const auto convert = [](const auto& self, json_object * object) -> Data {
    if( object == nullptr ) {
      return Data::null();
    }

    switch( json_object_get_type(object) ) {
      case json_type_null:
        return Data::null();
      case json_type_boolean:
        return Data::boolean(json_object_get_boolean(object) != 0);
      case json_type_double: {
        Data value(std::make_unique<Storage>(
            json_object_get_double(object)));
        value.storage_->scalarSpelling = json_object_get_string(object);
        return value;
      }
      case json_type_int: {
        const std::int64_t value = json_object_get_int64(object);
        if( std::to_string(value) != json_object_get_string(object) ) {
          throw Exception("JSON integer is outside the supported range");
        }
        return Data::integer(value);
      }
      case json_type_string:
        return Data::string(std::string(
            json_object_get_string(object), static_cast<std::size_t>(
                json_object_get_string_len(object))));
      case json_type_object: {
        Data value = Data::object();
        json_object_object_foreach(object, key, child)
        {
          value.set(key, self(self, child));
        }
        return value;
      }
      case json_type_array: {
        const std::size_t childCount = json_object_array_length(object);
        if( childCount > static_cast<std::size_t>(
                std::numeric_limits<int>::max()) ) {
          throw Exception("JSON array is too large");
        }
        Data value(Data::TypeArray, static_cast<int>(childCount));
        for( std::size_t i = 0; i < childCount; ++i ) {
          value.push_back(self(
              self, json_object_array_get_idx(object, i)));
        }
        return value;
      }
    }

    throw Exception("Unknown json type");
  };

  return convert(convert, result.get());
}

Data * Data::createFromJSON(const char * string)
{
  return new Data(fromJSON(string));
}
#else
Data Data::fromJSON(const char *)
{
  throw Exception("JSON support not enabled");
}

Data * Data::createFromJSON(const char * string)
{
  return new Data(fromJSON(string));
}
#endif

#if defined(MUSTACHE_HAVE_LIBYAML)
namespace {

Data createFromYAMLNode(
    yaml_document_t * document, yaml_node_t * node)
{
  if( node == nullptr ) {
    throw Exception("Missing yaml node");
  }

  switch( node->type ) {
    case YAML_SCALAR_NODE:
      return Data::string(std::string(
          reinterpret_cast<char *>(node->data.scalar.value),
          node->data.scalar.length));
    case YAML_MAPPING_NODE: {
      Data result = Data::object();
      for( yaml_node_pair_t * pair = node->data.mapping.pairs.start;
          pair < node->data.mapping.pairs.top; ++pair ) {
        yaml_node_t * keyNode =
            yaml_document_get_node(document, pair->key);
        yaml_node_t * valueNode =
            yaml_document_get_node(document, pair->value);
        if( keyNode == nullptr || keyNode->type != YAML_SCALAR_NODE ) {
          throw Exception("Invalid yaml object key");
        }
        std::string key(
            reinterpret_cast<char *>(keyNode->data.scalar.value),
            keyNode->data.scalar.length);
        result.set(std::move(key), createFromYAMLNode(document, valueNode));
      }
      return result;
    }
    case YAML_SEQUENCE_NODE: {
      const std::size_t length = static_cast<std::size_t>(
          node->data.sequence.items.top - node->data.sequence.items.start);
      if( length > static_cast<std::size_t>(
              std::numeric_limits<int>::max()) ) {
        throw Exception("YAML array is too large");
      }
      Data result(Data::TypeArray, static_cast<int>(length));
      for( yaml_node_item_t * item = node->data.sequence.items.start;
          item < node->data.sequence.items.top; ++item ) {
        result.push_back(createFromYAMLNode(
            document, yaml_document_get_node(document, *item)));
      }
      return result;
    }
    default:
      throw Exception("Unknown yaml type");
  }
}

} // namespace

Data Data::fromYAML(const char * string)
{
  if( string == nullptr ) {
    throw Exception("Missing YAML data");
  }

  yaml_parser_t parser;
  yaml_document_t document;
  if( yaml_parser_initialize(&parser) == 0 ) {
    throw Exception("Failed to initialize yaml parser");
  }

  const unsigned char * input =
      reinterpret_cast<const unsigned char *>(string);
  yaml_parser_set_input_string(&parser, input, std::char_traits<char>::length(string));
  if( yaml_parser_load(&parser, &document) == 0 ) {
    yaml_parser_delete(&parser);
    throw Exception("Failed to parse yaml document");
  }

  try {
    yaml_node_t * root = yaml_document_get_root_node(&document);
    if( root == nullptr ) {
      throw Exception("Empty yaml document");
    }
    Data data = createFromYAMLNode(&document, root);
    yaml_document_delete(&document);
    yaml_parser_delete(&parser);
    return data;
  } catch( ... ) {
    yaml_document_delete(&document);
    yaml_parser_delete(&parser);
    throw;
  }
}

Data * Data::createFromYAML(const char * string)
{
  return new Data(fromYAML(string));
}
#else
Data Data::fromYAML(const char *)
{
  throw Exception("YAML support not enabled");
}

Data * Data::createFromYAML(const char * string)
{
  return new Data(fromYAML(string));
}
#endif

} // namespace mustache
