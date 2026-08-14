#include "compiled_template.hpp"

#include "mustache.hpp"

#include <utility>

namespace mustache {

struct CompiledTemplate::State {
  Node root;
};

CompiledTemplate::CompiledTemplate() noexcept = default;

CompiledTemplate::CompiledTemplate(const CompiledTemplate& other) noexcept =
    default;

CompiledTemplate& CompiledTemplate::operator=(
    const CompiledTemplate& other) noexcept = default;

CompiledTemplate::CompiledTemplate(CompiledTemplate&& other) noexcept =
    default;

CompiledTemplate& CompiledTemplate::operator=(
    CompiledTemplate&& other) noexcept = default;

CompiledTemplate::~CompiledTemplate() = default;

CompiledTemplate::CompiledTemplate(
    std::shared_ptr<const State> state) noexcept :
    state(std::move(state))
{
}

bool CompiledTemplate::empty() const noexcept
{
  return !state;
}

CompiledTemplate::operator bool() const noexcept
{
  return !empty();
}

CompiledTemplate Mustache::compile(std::string_view source)
{
  return compile(source, Tokenizer::Limits());
}

CompiledTemplate Mustache::compile(
    std::string_view source, const Tokenizer::Limits& limits)
{
  std::shared_ptr<CompiledTemplate::State> compiled =
      std::make_shared<CompiledTemplate::State>();
  tokenizer.tokenize(source, &compiled->root, limits);
  return CompiledTemplate(std::move(compiled));
}

std::string Mustache::render(
    const CompiledTemplate& compiled, const Data& data) const
{
  return render(compiled, data, PartialMap());
}

std::string Mustache::render(const CompiledTemplate& compiled, const Data& data,
    const PartialMap& partials) const
{
  if( compiled.empty() ) {
    throw Exception("Empty compiled template");
  }

  std::string output;
  Renderer compiledRenderer;
  compiledRenderer.init(&compiled.state->root, &data, NULL, &output);
  compiledRenderer.setPartialResolver(
      [&partials](const std::string& name) -> const Node * {
        PartialMap::const_iterator partial = partials.find(name);
        if( partial == partials.end() ) {
          return NULL;
        }
        if( partial->second.empty() ) {
          throw Exception("Empty compiled partial");
        }
        return &partial->second.state->root;
      });
  compiledRenderer.render();
  return output;
}

CompiledTemplate compile(std::string_view source)
{
  Mustache mustache;
  return mustache.compile(source);
}

CompiledTemplate compile(
    std::string_view source, const Tokenizer::Limits& limits)
{
  Mustache mustache;
  return mustache.compile(source, limits);
}

std::string render(const CompiledTemplate& compiled, const Data& data)
{
  Mustache mustache;
  return mustache.render(compiled, data);
}

std::string render(const CompiledTemplate& compiled, const Data& data,
    const PartialMap& partials)
{
  Mustache mustache;
  return mustache.render(compiled, data, partials);
}

} // namespace mustache
