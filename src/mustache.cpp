
#include <stdio.h>

#include "mustache.hpp"


const char * mustache_version() {
  return MUSTACHE_PACKAGE_VERSION;
}

int mustache_version_int() {
  int maj = 0, min = 0, rev = 0;
  sscanf(MUSTACHE_PACKAGE_VERSION, "%d.%d.%d", &maj, &min, &rev);
  return (rev + (100 * min) + (10000 * maj));
}


namespace mustache {


void Mustache::tokenize(std::string * tmpl, Node * root)
{
  tokenizer.tokenize(tmpl, root);
}

void Mustache::tokenize(std::string * tmpl, Node * root,
    const Tokenizer::Limits& limits)
{
  tokenizer.tokenize(tmpl, root, limits);
}

void Mustache::tokenize(std::string_view tmpl, Node * root)
{
  tokenizer.tokenize(tmpl, root);
}

void Mustache::tokenize(std::string_view tmpl, Node * root,
    const Tokenizer::Limits& limits)
{
  tokenizer.tokenize(tmpl, root, limits);
}

void Mustache::render(const Node * node, const Data * data,
    const Node::Partials * partials, std::string * output)
{
  renderer.init(node, data, partials, output);
  renderer.render();
}

void Mustache::render(const Node * node, const Data * data,
    const Node::Partials * partials, std::string * output,
    const RenderLimits& limits)
{
  renderer.init(node, data, partials, output, limits);
  renderer.render();
}

    


} // namespace Mustache
