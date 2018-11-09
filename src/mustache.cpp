
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

void Mustache::render(Node * node, Data * data, Node::Partials * partials, std::string * output)
{
  renderer.init(node, data, partials, output);
  renderer.render();
}

    


} // namespace Mustache
