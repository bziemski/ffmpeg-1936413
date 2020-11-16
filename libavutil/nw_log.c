
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include "common.h"
#include "nw_log.h"

void get_nw_timestamp(char *out_str)
{
    long ms;  // Milliseconds
    time_t s; // Seconds
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    s = spec.tv_sec;
    ms = round(spec.tv_nsec / 1.0e6); // Convert nanoseconds to milliseconds
    if (ms > 999)
    {
        s++;
        ms = 0;
    }
    char buf[256];
    struct tm *ptm = localtime(&s);
    strftime(buf, 256, "%FT%T", ptm);

    char str[] = "[%s.%03ld]";
    sprintf(out_str, str, buf, ms);
    // printf("produced timestamp: %s\n", out_str);
    // return str2;
    return;
}
