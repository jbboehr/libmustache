#ifndef MUSTACHE_COMPILED_TEMPLATE_HPP
#define MUSTACHE_COMPILED_TEMPLATE_HPP

#include <map>
#include <memory>
#include <string>

namespace mustache {

class Mustache;

/*! \class CompiledTemplate
    \brief Opaque, immutable compiled template handle.

    Compiled templates own their parsed representation through immutable
    shared state. Copies are inexpensive and remain valid independently of the
    source string and the Mustache instance that compiled them.
*/
class CompiledTemplate {
  public:
    CompiledTemplate() noexcept;
    CompiledTemplate(const CompiledTemplate& other) noexcept;
    CompiledTemplate& operator=(const CompiledTemplate& other) noexcept;
    CompiledTemplate(CompiledTemplate&& other) noexcept;
    CompiledTemplate& operator=(CompiledTemplate&& other) noexcept;
    ~CompiledTemplate();

    //! Returns true when this handle contains no compiled template.
    bool empty() const noexcept;

    //! Tests whether this handle contains a compiled template.
    explicit operator bool() const noexcept;

  private:
    struct State;

    explicit CompiledTemplate(std::shared_ptr<const State> state) noexcept;

    std::shared_ptr<const State> state;

    friend class Mustache;
};

//! A map of named, independently owned compiled partial templates.
typedef std::map<std::string, CompiledTemplate> PartialMap;

} // namespace mustache

#endif
