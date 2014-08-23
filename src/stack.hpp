
#ifndef MUSTACHE_STACK_HPP
#define MUSTACHE_STACK_HPP

#include "mustache_config.h"

namespace mustache {

#ifndef MUSTACHE_STACK_DEFAULT_MAXSIZE
#define MUSTACHE_STACK_DEFAULT_MAXSIZE 96
#endif

/*! \class Stack
    \brief Stack.

    This class is used to implement stacks
*/
template <class T, int S = MUSTACHE_STACK_DEFAULT_MAXSIZE>
class Stack {
  public:
    static const int MAXSIZE = S;

  private:
    //! The current size
    int _size;

    //! The data
    T _stack[Stack::MAXSIZE];

  public:
    //! Constructor
    Stack() : _size(0) {};

    //! Clear the data stack
    void clear() {
      _size = 0;
    }

    //! Add an element onto the top of the stack
    void push_back(T data) {
#ifndef MUSTACHE_STACK_UNCHECKED
      if( _size < 0 || _size >= Stack::MAXSIZE ) {
        throw Exception("Reached max stack size");
      }
#endif
      _stack[_size] = data;
      _size++;
    }

    //! Pop an element off the top of the stack
    T pop_back() {
#ifndef MUSTACHE_STACK_UNCHECKED
      if( _size <= 0 ) {
        throw Exception("Reached bottom of stack");
      }
#endif
      T v = back();
      _size--;
      //_stack[_size] = NULL;
      return v;
    };

    //! Get the top of the stack
    T back() {
#ifndef MUSTACHE_STACK_UNCHECKED
      if( _size <= 0 ) {
        throw Exception("Reached bottom of stack");
      }
#endif
      return _stack[_size - 1];
    };

    T backOffset(int of) {
#ifndef MUSTACHE_STACK_UNCHECKED
      if( _size <= of ) {
        throw Exception("Reached bottom of stack");
      }
#endif
      return _stack[_size - 1 - of];
    }

    //! Get the size of the stack
    int size() {
      return _size;
    };

    //! Gets a pointer to the beginning of the stack
    T * begin() {
      return _stack;
    };

    //! Gets a pointer to the end of the stack
    T * end() {
      return (_stack + _size - 1);
    };
};

}


#endif
