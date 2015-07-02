#include "log.h"

static FILE *logfd;

void log_init(const char *logpath) {
    logfd = fopen(logpath, "w");
    if (logfd == NULL) {
        printf("LancerFS Error: connot find log file\n");
    }
}

void log_msg(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    vfprintf(logfd, format, ap);
    fflush(logfd);
}

void log_destroy() {
    fclose(logfd);
}
