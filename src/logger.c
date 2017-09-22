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