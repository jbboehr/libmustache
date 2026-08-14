#ifndef MUSTACHE_DATA_HPP
#define MUSTACHE_DATA_HPP

#include "mustache_config.h"

#include <cstdint>
#include <list>
#include <memory>
#include <string>
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
    Data();

    /*! Construct an empty value of an existing compatibility type.

        The size is used only to reserve string or array capacity. New code
        should prefer the named factories below.
    */
    Data(Type type, int size);

    Data(const Data& other);
    Data& operator=(const Data& other);
    Data(Data&& other) noexcept;
    Data& operator=(Data&& other) noexcept;
    ~Data();

    static Data null();
    static Data boolean(bool value);
    static Data integer(std::int64_t value);
    static Data floating(double value);
    static Data string(std::string value);
    static Data list(List values = List());
    static Data array(Array values = Array());
    static Data object(Map values = Map());
    static Data lambda(std::unique_ptr<Lambda> value);
    static Data sharedLambda(std::shared_ptr<Lambda> value);

    //! Compatibility initializer. Replacement is transactional.
    void init(Type type, int size);

    Type type() const noexcept;
    int isEmpty() const noexcept;

    String& stringValue();
    const String& stringValue() const;
    bool booleanValue() const;
    std::int64_t integerValue() const;
    double floatingValue() const;
    const List& listItems() const;
    const Array& arrayItems() const;
    const Map& objectItems() const;
    Lambda * lambdaValue() const noexcept;

    /*! Return the scalar spelling used for interpolation.

        Null and false return an empty string. Containers and lambdas reject
        conversion.
    */
    std::string toString() const;

    //! Add or replace an object member.
    Data& set(std::string key, Data value);

    //! Append to a list or array value.
    Data& push_back(Data value);

    /*! Find an object member without transferring ownership.

        Borrowed pointers and container references must not be retained across
        structural changes to the owning Data tree. The tree must not be
        structurally modified while a renderer is using it.
    */
    const Data * find(const std::string& key) const noexcept;

    //! Parse into a value. The pointer-returning forms remain for compatibility.
    static Data fromJSON(const char * string);
    static Data * createFromJSON(const char * string);
    static Data fromYAML(const char * string);
    static Data * createFromYAML(const char * string);

    void swap(Data& other) noexcept;

  private:
    struct Storage;
    std::unique_ptr<Storage> storage_;

    explicit Data(std::unique_ptr<Storage> storage) noexcept;
    static std::unique_ptr<Storage> makeStorage(Type type, int size);
    Storage& requireStorage(Type expected, const char * description);
    const Storage& requireStorage(
        Type expected, const char * description) const;
};

} // namespace mustache

#endif
