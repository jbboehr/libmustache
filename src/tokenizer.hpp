
#ifndef MUSTACHE_TOKENIZER_HPP
#define MUSTACHE_TOKENIZER_HPP

#include "mustache_export.hpp"

#include <cstddef>
#include <iostream>
#include <memory>
#include <sstream>
#include <stack>
#include <string>
#include <string_view>
#include <utility>

#include "exception.hpp"
#include "node.hpp"
#include "utils.hpp"

namespace mustache {

/*! \class Tokenizer
    \brief Tokenizes a template

    This class tokenizes a string template and returns a token tree.
*/
class Tokenizer {
  private:
    //! The default initial start string
    std::string _startSequence;

    //! The default initial stop string
    std::string _stopSequence;

    //! Whether to escape HTML by default
    bool _escapeByDefault;

  public:
    /*! \struct Limits
        \brief Resource limits for parsing templates.

        Every field is an enforced maximum. A zero value therefore rejects
        any input that consumes that resource; zero never means unlimited.
        Nesting depth counts open sections and excludes the root node.
    */
    struct Limits {
        std::size_t maxInputBytes;
        std::size_t maxNestingDepth;
        std::size_t maxNodes;
        std::size_t maxTagBytes;
        std::size_t maxDelimiterBytes;

        MUSTACHE_API Limits();
    };

    //! Constructor
    Tokenizer() :
        _startSequence("{{"),
        _stopSequence("}}"),
        _escapeByDefault(true) {};

    //! Sets the start sequence
    MUSTACHE_API void setStartSequence(const std::string& start);

    //! Sets the start sequence from an explicitly sized view
    MUSTACHE_API void setStartSequence(std::string_view start);

    //! Sets the start sequence
    MUSTACHE_API void setStartSequence(const char * start, int len = 0);

    //! Sets the stop sequence
    MUSTACHE_API void setStopSequence(const std::string& stop);

    //! Sets the stop sequence from an explicitly sized view
    MUSTACHE_API void setStopSequence(std::string_view stop);

    //! Sets the stop sequence
    MUSTACHE_API void setStopSequence(const char * stop, int len = 0);

    //! Sets whether to escape HTML by default
    MUSTACHE_API void setEscapeByDefault(bool flag);

    //! Gets the current start sequence
    MUSTACHE_API const std::string& getStartSequence();

    //! Gets the current stop sequence
    MUSTACHE_API const std::string& getStopSequence();

    //! Gets whether to escape HTML by default
    MUSTACHE_API bool getEscapeByDefault();

    //! Tokenizes the given string template
    MUSTACHE_API void tokenize(std::string * tmpl, Node * root, bool escapeOutput = false);

    //! Tokenizes the given string template with resource limits
    MUSTACHE_API void tokenize(std::string * tmpl, Node * root, const Limits& limits, bool escapeOutput = false);

    //! Tokenizes an explicitly sized template view
    MUSTACHE_API void tokenize(std::string_view tmpl, Node * root, bool escapeOutput = false);

    //! Tokenizes an explicitly sized template view with resource limits
    MUSTACHE_API void tokenize(std::string_view tmpl, Node * root, const Limits& limits, bool escapeOutput = false);
};

} // namespace mustache

#endif
