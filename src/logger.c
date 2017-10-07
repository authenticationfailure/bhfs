/*
  BHFS: Black Hole Filesystem
  
  Copyright (C) 2017	   David Turco  	

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

#include <stdio.h>
#include <stdarg.h>

void bhfs_log(int level, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    
    printf("BHFS: ");
    vprintf(format, ap);
    printf("\n");

    va_end(ap);
}