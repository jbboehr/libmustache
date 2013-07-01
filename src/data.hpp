
#ifndef MUSTACHE_DATA_HPP
#define MUSTACHE_DATA_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <list>
#include <memory>
#include <map>
#include <string>
#include <vector>

#ifdef HAVE_CXX11
#include <unordered_map>
#else
#include <map>
#endif

#include "exception.hpp"

namespace mustache {

/*! \class Data
    \brief Associative data structure.

    This class implements a data structure similar to associative arrays in PHP
    or JSON.
*/
class Data {
  public:
    typedef std::auto_ptr<Data> Ptr;
    typedef std::string String;
#ifdef HAVE_CXX11
    typedef std::unordered_map<std::string,Data *> Map;
#else
    typedef std::map<std::string,Data *> Map;
#endif
    typedef std::list<Data *> List;
    typedef Data * Array;
    
    //! Enum of the different supported data types
    enum Type { 
      TypeNone = 0,
      TypeString = 1,
      TypeList = 2,
      TypeMap = 3,
      TypeArray = 4
    };
    
    //! The data type
    Data::Type type;
    
    //! The length of the data (only used for the array type)
    int length;
    
    //! The current string value
    Data::String * val;
    
    //! The current map value
    Data::Map data;
    
    //! The current array value (list)
    Data::List children;
    
    //! The current array value (array)
    Data::Array array;
    
    //! Constructor
    Data() : 
        type(Data::TypeNone),
        val(NULL) {};
    Data(Data::Type type, int size) : val(NULL) {
      init(type, size);
    };
    
    //! Destructor
    ~Data();
    
    //! Checks if the node is empty. The includes an empty value.
    int isEmpty();
    
    //! Initialize the data
    void init(Data::Type type, int size);
    
    //! Create from json
    static Data & createFromJSON(const char * string);
    
    //! Create from yaml
    static Data & createFromYAML(const char * string);
};

/*! \class DataStack
    \brief Data stack.

    This class is used to implement stack lookups in the renderer.
*/
class DataStack {
  public:
    //! The maximum size of the stack
#ifdef LIBMUSTACHE_DATA_STACK_MAXSIZE
    static const int MAXSIZE = LIBMUSTACHE_DATA_STACK_MAXSIZE;
#else
    static const int MAXSIZE = 32;
#endif
    
  private:
    //! The current size
    int _size;
    
    //! The data
    Data * _stack[DataStack::MAXSIZE];
    
  public:
    //! Constructor
    DataStack() : _size(0) {};
    
    //! Add an element onto the top of the stack
    void push_back(Data * data);
    
    //! Pop an element off the top of the stack
    void pop_back();
    
    //! Get the top of the stack
    Data * back();
    
    //! Get the size of the stack
    int size() { return _size; };
    
    //! Gets a pointer to the beginning of the stack
    Data ** begin();
    
    //! Gets a pointer to the end of the stack
    Data ** end();
};


} // namespace Mustache

#endif


