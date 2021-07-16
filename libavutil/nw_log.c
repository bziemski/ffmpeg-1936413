
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include "common.h"
#include "nw_log.h"
#include <sys/types.h>
#include <unistd.h>


static const char* file_path = "globals.txt";

void nw_set(int64_t value){
    FILE *fp;
    
    char pid[20];   
    get_pid(pid);

    fp = fopen(pid, "w+");
    fprintf(fp, "%ld", value);
    fclose(fp);
}

void get_pid(char* pid_s){
    int pid = getpid();
    sprintf(pid_s, "globals_%d.txt", pid);
}

int64_t nw_get(){
    char pid[20];   
    get_pid(pid);

    FILE *fp;
   
//    printf(fp, "dflksdlkfsd: %s", pid);
   fp = fopen(pid, "r");
    if(!fp){
        return 0; 
    } 
   char buff[255];

   fscanf(fp, "%s", buff);
    int64_t x = atoi(buff);
   fclose(fp);

   //TODO: If not found return 0
    // return 0;
    return x;
}



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
