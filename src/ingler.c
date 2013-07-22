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

struct ingler {
    char dir[MAXPATHLEN];
    char prefix[MAXPATHLEN];
    size_t prefix_len;
};

static void
run_command(struct ingler* ingler, const char* name)
{
    if (strncmp(name, ingler->prefix, ingler->prefix_len) != 0) {
        return;
    }
    char cmd[MAXPATHLEN];
    snprintf(cmd, sizeof(cmd), "%s/%s", ingler->dir, name);

    struct stat sb;
    if (stat(cmd, &sb) != 0) {
        die(1, "failed to stat(2): %s: %s", cmd, strerror(errno));
    }
    int mode = S_IXUSR | S_IXGRP | S_IXOTH;
    if ((sb.st_mode & mode) != mode) {
        syslog(LOG_WARNING, "%s is not executable. ignored.", cmd);
        return;
    }

    FILE* fpin = popen(cmd, "r");
    if (fpin == NULL) {
        die(1, "failed to popen(3).");
    }
    char line[8192];
    while (fgets(line, sizeof(line), fpin) != NULL) {
        trim(line);
        syslog(LOG_INFO, "%s: %s", &name[ingler->prefix_len], line);
    }
    if (ferror(fpin)) {
        die(1, "failed to fgets(3): %s", strerror(errno));
    }
    pclose(fpin);
}

static void
run_jobs(struct ingler *ingler)
{
    DIR* dirp = opendir(ingler->dir);
    if (dirp == NULL) {
        die(1, "failed to opendir(3): %s", ingler->dir);
    }
    struct dirent* dp;
    while ((dp = readdir(dirp)) != NULL) {
        run_command(ingler, dp->d_name);
    }
    closedir(dirp);
}

static void
sigterm_handler(int sig)
{
    terminated = true;
}

static void
compute_directory(struct ingler* ingler, const char* prog)
{
    if (prog[0] == '/') {
        char buf[MAXPATHLEN];
        strcpy(buf, prog);
        strcpy(ingler->dir, dirname(buf));
        return;
    }

    char cwd[MAXPATHLEN];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        die(1, "failed to getcwd(3): %s", strerror(errno));
    }
    char buf[MAXPATHLEN];
    snprintf(buf, sizeof(buf), "%s/%s", cwd, prog);
    strcpy(ingler->dir, dirname(buf));
}

int
main(int argc, const char* argv[])
{
    openlog(getprogname(), LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO, "starting.");

    if (signal(SIGTERM, sigterm_handler) == SIG_ERR) {
        die(1, "failed to signal(2): %s", strerror(errno));
    }

    struct ingler ingler;

    compute_directory(&ingler, argv[0]);

    snprintf(ingler.prefix, sizeof(ingler.prefix), "%s.", getprogname());
    ingler.prefix_len = strlen(ingler.prefix);

    if (daemon(0, 1) != 0) {
        die(1, "failed to daemon(3): %s", strerror(errno));
    }

    while (!terminated) {
        run_jobs(&ingler);
        sleep(1);
    }

    syslog(LOG_INFO, "stopped.");
    closelog();

    return 0;
}

/**
 * vim: tabstop=4 shiftwidth=4 expandtab softtabstop=4
 */
