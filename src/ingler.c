#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

static bool terminated = false;

static void
die(int status, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsyslog(LOG_ERR, fmt, ap);
    va_end(ap);

    exit(status);
}

static void
trim(char* s)
{
    size_t len = strlen(s);
    if (len == 0) {
        return;
    }

    char* pc = &s[len - 1];
    while ((*pc == '\n') || (*pc == '\r')) {
        *pc = '\0';
        pc--;
    }
}

static void
job_main(const char* prog)
{
    char buf[MAXPATHLEN];
    strcpy(buf, prog);
    char* dir = dirname(buf);
    DIR* dirp = opendir(dir);
    if (dirp == NULL) {
        die(1, "failed opendir(3): %s", dir);
    }
    char prefix[MAXPATHLEN];
    snprintf(prefix, sizeof(prefix), "%s.", getprogname());
    size_t prefix_len = strlen(prefix);
    struct dirent* dp;
    while ((dp = readdir(dirp)) != NULL) {
        const char* name = dp->d_name;
        if (strncmp(name, prefix, prefix_len) != 0) {
            continue;
        }
        char cmd[8192];
        snprintf(cmd, sizeof(cmd), "%s/%s", dir, name);
        struct stat sb;
        if (stat(cmd, &sb) != 0) {
            die(1, "failed to stat(2): %s: %s", cmd, strerror(errno));
        }
        if ((sb.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) == 0) {
            continue;
        }
        FILE* fpin = popen(cmd, "r");
        if (fpin == NULL) {
            die(1, "failed to popen(3).");
        }
        char line[8192];
        while (fgets(line, sizeof(line), fpin) != NULL) {
            trim(line);
            syslog(LOG_INFO, "%s: %s", &name[prefix_len], line);
        }
        pclose(fpin);
    }
    closedir(dirp);
}

static void
sigterm_handler(int sig)
{
    terminated = true;
}

int
main(int argc, const char* argv[])
{
    openlog(getprogname(), LOG_PID, LOG_DAEMON);

    if (signal(SIGTERM, sigterm_handler) == SIG_ERR) {
        die(1, "failed to signal(2): %s", strerror(errno));
    }

    char cwd[MAXPATHLEN];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        die(1, "failed getcwd(3): %s", strerror(errno));
    }
    if (daemon(0, 1) != 0) {
        die(1, "failed daemon(3): %s", strerror(errno));
    }
    char prog[MAXPATHLEN];
    snprintf(prog, sizeof(prog), "%s/%s", cwd, argv[0]);

    while (!terminated) {
        job_main(prog);
        sleep(1);
    }

    closelog();

    return 0;
}

/**
 * vim: tabstop=4 shiftwidth=4 expandtab softtabstop=4
 */
