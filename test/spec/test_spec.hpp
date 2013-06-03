
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define MAX_TEST_FILES 50

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <yaml.h>
#include <sys/stat.h>

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

void parse_file(char * fileData, int length);
