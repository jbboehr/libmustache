#ifndef MUSTACHE_DATA_HPP
#define MUSTACHE_DATA_HPP

#include "mustache_config.h"
#include "mustache_export.hpp"

#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "exception.hpp"
#include "lambda.hpp"

namespace mustache {

/*! \class Data
    \brief Owned value used as the rendering context.

    Data owns its strings, containers, and nested values. Lambda values use
    shared ownership so copying a Data tree has explicit, safe callback
    lifetime semantics.
*/
class Data {
  public:
    typedef std::string String;
    typedef std::unordered_map<std::string, Data> Map;
    typedef std::list<Data> List;
    typedef std::vector<Data> Array;

    /*! \struct ParseLimits
        \brief Resource limits for JSON and YAML parsing.

        Every field is an enforced maximum. A zero value therefore rejects
        any input that consumes that resource; zero never means unlimited.
        Nesting depth counts value nodes along a root-to-leaf path, including
        the root. YAML aliases count each expanded value and container entry.
        JSON applies the limits while SAX events construct the owned value
        tree. YAML checks them before its dependency constructs a complete
        document and again during conversion. Parsing also retains an
        implementation safety ceiling of 256 value nodes along any
        root-to-leaf path.
    */
    struct ParseLimits {
        std::size_t maxInputBytes;
        std::size_t maxNestingDepth;
        std::size_t maxNodes;
        std::size_t maxStringBytes;
        std::size_t maxContainerEntries;

        MUSTACHE_API ParseLimits();
    };

    //! Enum of the supported data types. Existing values retain their numbers.
    enum Type {
      TypeNone = 0,
      TypeString = 1,
      TypeList = 2,
      TypeMap = 3,
      TypeArray = 4,
      TypeLambda = 5,
      TypeBoolean = 6,
      TypeInteger = 7,
      TypeDouble = 8
    };

    //! Construct a null value.
    MUSTACHE_API Data();

    /*! Construct an empty value of an existing compatibility type.

        The size is used only to reserve string or array capacity. New code
        should prefer the named factories below.
    */
    MUSTACHE_API Data(Type type, int size);

    MUSTACHE_API Data(const Data& other);
    MUSTACHE_API Data& operator=(const Data& other);
    MUSTACHE_API Data(Data&& other) noexcept;
    MUSTACHE_API Data& operator=(Data&& other) noexcept;
    MUSTACHE_API ~Data();

    static MUSTACHE_API Data null();
    static MUSTACHE_API Data boolean(bool value);
    static MUSTACHE_API Data integer(std::int64_t value);
    static MUSTACHE_API Data floating(double value);
    static MUSTACHE_API Data string(std::string value);
    static MUSTACHE_API Data list(List values = List());
    static MUSTACHE_API Data array(Array values = Array());
    static MUSTACHE_API Data object(Map values = Map());
    static MUSTACHE_API Data lambda(std::unique_ptr<Lambda> value);
    static MUSTACHE_API Data sharedLambda(std::shared_ptr<Lambda> value);

    //! Compatibility initializer. Replacement is transactional.
    MUSTACHE_API void init(Type type, int size);

    MUSTACHE_API Type type() const noexcept;
    MUSTACHE_API int isEmpty() const noexcept;

    MUSTACHE_API String& stringValue();
    MUSTACHE_API const String& stringValue() const;
    MUSTACHE_API bool booleanValue() const;
    MUSTACHE_API std::int64_t integerValue() const;
    MUSTACHE_API double floatingValue() const;
    MUSTACHE_API const List& listItems() const;
    MUSTACHE_API const Array& arrayItems() const;
    MUSTACHE_API const Map& objectItems() const;
    MUSTACHE_API Lambda * lambdaValue() const noexcept;

    /*! Return the scalar spelling used for interpolation.

        Null and false return an empty string. Containers and lambdas reject
        conversion.
    */
    MUSTACHE_API std::string toString() const;

    //! Add or replace an object member.
    MUSTACHE_API Data& set(std::string key, Data value);

    //! Append to a list or array value.
    MUSTACHE_API Data& push_back(Data value);

    /*! Find an object member without transferring ownership.

        Borrowed pointers and container references must not be retained across
        structural changes to the owning Data tree. The tree must not be
        structurally modified while a renderer is using it.
    */
    MUSTACHE_API const Data * find(const std::string& key) const noexcept;

    //! Parse into a value. The pointer-returning forms remain for compatibility.
    static MUSTACHE_API Data fromJSON(const char * string);
    static MUSTACHE_API Data fromJSON(const char * string, const ParseLimits& limits);
    static MUSTACHE_API Data fromJSON(std::string_view string);
    static MUSTACHE_API Data fromJSON(std::string_view string, const ParseLimits& limits);
    static MUSTACHE_API Data * createFromJSON(const char * string);
    static MUSTACHE_API Data * createFromJSON(const char * string, const ParseLimits& limits);
    static MUSTACHE_API Data fromYAML(const char * string);
    static MUSTACHE_API Data fromYAML(const char * string, const ParseLimits& limits);
    static MUSTACHE_API Data fromYAML(std::string_view string);
    static MUSTACHE_API Data fromYAML(std::string_view string, const ParseLimits& limits);
    static MUSTACHE_API Data * createFromYAML(const char * string);
    static MUSTACHE_API Data * createFromYAML(const char * string, const ParseLimits& limits);

    MUSTACHE_API void swap(Data& other) noexcept;

  private:
    class JSONDataBuilder;
    struct Storage;
    std::unique_ptr<Storage> storage_;

    explicit Data(std::unique_ptr<Storage> storage) noexcept;
#if defined(__GNUC__) && !defined(_WIN32)
    __attribute__((visibility("hidden")))
#endif
    static Data parsedFloating(double value, std::string spelling);
#if defined(__GNUC__) && !defined(_WIN32)
    __attribute__((visibility("hidden")))
#endif
    static Data parseJSON(std::string_view string, const ParseLimits& limits);
    static std::unique_ptr<Storage> makeStorage(Type type, int size);
    Storage& requireStorage(Type expected, const char * description);
    const Storage& requireStorage(Type expected, const char * description) const;
};

} // namespace mustache

#endif
