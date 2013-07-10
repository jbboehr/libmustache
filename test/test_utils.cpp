
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cstdlib>
#include <cstdio>

#include "utils.hpp"

int main( int argc, char * argv[] )
{
  std::string case1 = "a.b.c";
  std::vector<std::string> result1;
  mustache::explode(".", case1, &result1);
  if( result1.size() != 3 ) {
    fprintf(stdout, "Failed, expected three parts, got %lu\n", result1.size());
    return 1;
  }
  if( result1[0].compare("a") != 0 ) {
    fprintf(stdout, "Failed, expected part 1 to be 'a', got '%s'\n", result1[0].c_str());
    return 1;
  }
  if( result1[1].compare("b") != 0 ) {
    fprintf(stdout, "Failed, expected part 2 to be 'b', got '%s'\n", result1[1].c_str());
    return 1;
  }
  if( result1[2].compare("c") != 0 ) {
    fprintf(stdout, "Failed, expected part 3 to be 'c', got '%s'\n", result1[2].c_str());
    return 1;
  }
  
  return 0;
}

