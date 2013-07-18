
#include "mustache.hpp"


const char * mustache_version() {
  return LIBMUSTACHE_VERSION;
}

int mustache_version_int() {
  return LIBMUSTACHE_VERSION_INT;
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

//! Compile a node
void Mustache::compile(Node * node, uint8_t ** codes, size_t * length)
{
  std::vector<uint8_t> * vector = compiler.compile(node);
  Compiler::vectorToBuffer(vector, codes, length);
}

//! Compile a node (with partials)
void Mustache::compile(Node * node, Node::Partials * partials, uint8_t ** codes, size_t * length)
{
  std::vector<uint8_t> * vector = compiler.compile(node, partials);
  Compiler::vectorToBuffer(vector, codes, length);
}

//! Execute the VM
void Mustache::execute(uint8_t * codes, int length, Data * data, std::string * output)
{
  vm.execute(codes, length, data, output);
}
    


} // namespace Mustache
