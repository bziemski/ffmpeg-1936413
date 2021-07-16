#ifndef NW_LOG
#define NW_LOG

// #include <stdint.h>
// #include <time.h>

void nw_set(int64_t value);
void get_pid(char* pid_s);
int64_t nw_get();

/**
 * Get current timestamp log.
 */
void get_nw_timestamp(char *out_str);

#endif /* NW_LOG */
