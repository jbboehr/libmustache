
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "mustache.hpp"

#define MUSTACHE_BIN_MODE_COMPILE 1
#define MUSTACHE_BIN_MODE_EXECUTE 2

static int error = 0;
static int mode = 0;
static int number = 1;

static char * inputDataFileName = NULL;
static char * inputTemplateFileName = NULL;
static char * outputFileName = NULL;

static void showUsage();

int main( int argc, char * argv[] )
{
  // Process arguments
  int curopt;
  int numopt = 0;
  opterr = 0;
  
  while( (curopt = getopt(argc, argv, "hced:o:t:")) != -1 ) {
    numopt++;
    switch( curopt ) {
      case 'c':
        mode = MUSTACHE_BIN_MODE_COMPILE;
        break;
      case 'e':
        mode = MUSTACHE_BIN_MODE_EXECUTE;
        break;
        
      case 'd':
        inputDataFileName = optarg;
        break;
      case 'o':
        outputFileName = optarg;
        break;
      case 't':
        inputTemplateFileName = optarg;
        break;
        
      case '?':
        if( optopt == 'd' || optopt == 'o' || optopt == 't' ) {
          fprintf(stderr, "Option -%c requires an argument.\n", optopt);
        } else if( isprint(optopt) ) {
          fprintf(stderr, "Unknown option `-%c'.\n", optopt);
        } else {
          fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
        }
        return 1;
        break;
      
      default:
        error = 1;
      case 'h':
        showUsage();
        goto error;
        break;
    }
  }
  
  // Must specify a mode
  if( mode != MUSTACHE_BIN_MODE_COMPILE && mode != MUSTACHE_BIN_MODE_EXECUTE ) {
    error = 1;
    fprintf(stderr, "Must specify either -c (compile) or -e (execute)\n");
    showUsage();
    goto error;
  }
  
  // Must have input file
  if( inputTemplateFileName == NULL ) {
    error = 1;
    fprintf(stderr, "Must specify an input file with option -t\n");
    showUsage();
    goto error;
  }
  
error:
  return error;
}

static void showUsage()
{
  fprintf(stdout, "Usage: mustache -e -t <template file> -d <data file> (-o <output file>)\n");
  fprintf(stdout, "       mustache -c -t <template file> (-o <output file>)\n");
  fprintf(stdout, "\n");
  fprintf(stdout, "Modes:\n");
  fprintf(stdout, "    -c, Compile\n");
  fprintf(stdout, "    -e, Execute\n");
  fprintf(stdout, "\n");
  fprintf(stdout, "Options:\n");
  fprintf(stdout, "    -d, Input data file\n");
  fprintf(stdout, "    -o, Output file\n");
  fprintf(stdout, "    -t, Input template file\n");
  fprintf(stdout, "\n");
  fprintf(stdout, "Supported data formats:\n");
  fprintf(stdout, "    ");
#if defined(HAVE_LIBJSON) || defined(HAVE_LIBJANSSON)
  fprintf(stdout, "json ");
#endif
#ifdef HAVE_LIBYAML
  fprintf(stdout, "yaml ");
#endif
  fprintf(stdout, "\n");
}
