//
// Created by haruue on 17-6-28.
//


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <stdint.h>

#define TRUE 1
#define FALSE 0

#define EXIT_FATAL_PM_PATH 1
#define EXIT_FATAL_PM_PATH_MALFORMED 2
#define EXIT_FATAL_SET_CLASSPATH 3
#define EXIT_FATAL_FORK 4
#define EXIT_FATAL_APP_PROCESS 5
#define EXIT_WARN_OPEN_PROC 6
#define EXIT_WARN_START_TIMEOUT 7
#define EXIT_WARN_SERVER_STOP 8
#define ExIT_FATAL_KILL_OLD_SERVER 9

char *trimCRLF(char *str) {
    char *p;
    p = strchr(str, '\r');
    if (p != NULL) {
        *p = '\0';
    }
    p = strchr(str, '\n');
    if (p != NULL) {
        *p = '\0';
    }
    return str;
}

char *getApkPath(char *buffer) {
    FILE *file = popen("pm path moe.shizuku.privileged.api", "r");
    if (file != NULL) {
        fgets(buffer, (PATH_MAX + 11) * sizeof(char), file);
    } else {
        perror("fatal: can't invoke `pm path'\n");
        fflush(stderr);
        exit(EXIT_FATAL_PM_PATH);
    }
    trimCRLF(buffer);
    buffer = strchr(buffer, ':');
    if (buffer == NULL) {
        perror("fatal: can't get apk path, have you installed Shizuku Manager? Get it from https://play.google.com/store/apps/details?id=moe.shizuku.privileged.api\n");
        fflush(stderr);
        exit(EXIT_FATAL_PM_PATH_MALFORMED);
    }
    buffer++;
    printf("info: apk installed path: %s\n", buffer);
    fflush(stdout);
    return buffer;
}

void setClasspathEnv(char *path) {
    if (setenv("CLASSPATH", path, TRUE)) {
        perror("fatal: can't set CLASSPATH\n");
        fflush(stderr);
        exit(EXIT_FATAL_SET_CLASSPATH);
    }
    printf("info: CLASSPATH=%s\n", path);
    fflush(stdout);
}

pid_t getRikkaServerPid() {
    DIR *procDir = opendir("/proc");
    if (procDir != NULL) {
        struct dirent *pidDirent;
        while ((pidDirent = readdir(procDir)) != NULL) {
            uint32_t pid = (uint32_t) atoi(pidDirent->d_name);
            if (pid != 0) {  // skip non number directory or files
                char cmdlinePath[PATH_MAX];
                sprintf(cmdlinePath, "/proc/%d/cmdline", pid);
                cmdlinePath[PATH_MAX - 1] = '\0';
                FILE *cmdlineFile;
                if ((cmdlineFile = fopen(cmdlinePath, "r")) != NULL) {
                    char cmdline[50];
                    fread(cmdline, 1, 50, cmdlineFile);
                    cmdline[49] = '\0';
                    fclose(cmdlineFile);
                    if (strstr(cmdline, "rikka_server") != NULL) {
                        return (pid_t) pid;
                    }
                }
            }
        }
        return 0;
    } else {
        perror("\nwarn: can't open /proc, please check by yourself.\n");
        fflush(stderr);
        exit(EXIT_WARN_OPEN_PROC);
    }
}

void killOldServer() {
    pid_t pid = getRikkaServerPid();
    if (pid != 0) {
        printf("info: old rikka_server found, killing\n");
        fflush(stdout);
        if (kill(pid, SIGINT) != 0) {
            perror("fatal: can't kill old server, if you started it by root, please invoke this command:\n\n\t\t");
            fflush(stderr);
            char killByRootCommand[100];
            sprintf(killByRootCommand, "adb shell su -c \"kill %d\"", pid);
            killByRootCommand[99] = '\0';
            perror(killByRootCommand);
            fflush(stderr);
            perror("\n\n");
            fflush(stderr);
            exit(ExIT_FATAL_KILL_OLD_SERVER);
        }
    } else {
        printf("info: no old rikka_server found.\n");
        fflush(stdout);
    }
}

int main(int argc, char **argv) {
    printf("info: starter begin\n");
    fflush(stdout);
    killOldServer();
    char buffer[PATH_MAX + 11];
    char *path = getApkPath(buffer);
    pid_t pid = fork();
    if (pid == 0) {
        setClasspathEnv(path);
        char *appProcessArgs[] = {
                "/system/bin/app_process",
                "/system/bin",
                "--nice-name=rikka_server",
                "moe.shizuku.server.Server",
                NULL
        };
        if (execvp(appProcessArgs[0], appProcessArgs)) {
            char err[100];
            sprintf(err, "fatal: can't invoke app_process, errno=%d\n", errno);
            err[99] = '\0';
            perror(err);
            fflush(stderr);
            exit(EXIT_FATAL_APP_PROCESS);
        }
    } else if (pid == -1) {
        perror("fatal: can't fork\n");
        fflush(stderr);
        exit(EXIT_FATAL_FORK);
    } else {
        printf("info: child process forked, pid=%d\n", pid);
        fflush(stdout);
        printf("info: check rikka_server start\n");
        fflush(stdout);
        int rikkaServerPid;
        int count = 0;
        while ((rikkaServerPid = getRikkaServerPid()) == 0) {
            printf(".");
            fflush(stdout);
            sleep(1);
            count++;
            if (count > 10) {
                perror("\nwarn: timeout but can't get pid of rikka_server.\n");
                fflush(stderr);
                exit(EXIT_WARN_START_TIMEOUT);
            }
        }
        printf("\ninfo: check rikka_server stable\n");
        fflush(stdout);
        count = 0;
        while ((rikkaServerPid = getRikkaServerPid()) != 0) {
            printf(".");
            fflush(stdout);
            sleep(1);
            count++;
            if (count > 5) {
                printf("\ninfo: rikka_server started.");
                fflush(stdout);
                exit(EXIT_SUCCESS);
            }
        }
        perror("\nwarn: rikka_server stopped after started.\n");
        fflush(stderr);
        exit(EXIT_WARN_SERVER_STOP);
    }
}
