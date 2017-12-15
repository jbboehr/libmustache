
#include "mustache_config.h"

#include <cstdlib>
#include <cstdio>

#include "utils.hpp"
#include "test_spec.hpp"

int testGH46() {
  std::string tmpl = "a{{ <html> }}b";
  
  // Tokenize
  mustache::Mustache mustache;
  mustache::Node root;
  mustache.tokenize(&tmpl, &root);
  mustache::Data data;

  std::string output;
  mustache.render(&root, &data, NULL, &output);

  if( output.compare("ab") != 0 ) {
    fprintf(stdout, "Failed, expected output to be 'ab', got '%s'\n", output.c_str());
    return 1;
  }

  return 0;
}

int main( int argc, char * argv[] )
{
  int ret = 0;
  ret += testGH46();
  return ret;
}

