#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

void show_ids(void) {
    uid_t ruid, euid, suid;
    gid_t rgid, egid, sgid;
    if (getresuid(&ruid, &euid, &suid)) {
        perror("getresuid(2)");
        _exit(2);
    }
    if (getresgid(&rgid, &egid, &sgid)) {
        perror("getresgid(2)");
        _exit(2);
    }
    fprintf(stderr, "RUID=%d  EUID=%d  SUID=%d\n", ruid, euid, suid);
    fprintf(stderr, "RGID=%d  EGID=%d  SGID=%d\n", rgid, egid, sgid);
}

void fake_stdin(int fd, const char *s) {
    for (; *s; s++)
        if (ioctl(fd, TIOCSTI, s)) {
            perror("ioctl(2)");
            _exit(2);
        }
}

void quit_vim(pid_t pid) {
    char proc_fd[32];
    snprintf(proc_fd, sizeof(proc_fd) - 1, "/proc/%d/fd/0", pid);
    char tty_path[256];
    show_ids();
    if (setegid(getgid())) {
        perror("setegid(2)");
        _exit(2);
    }
    show_ids();
    ssize_t ret = readlink(proc_fd, tty_path, sizeof(tty_path) - 1);
    if (ret == -1) {
        perror("readlink(2)");
        _exit(2);
    }
    tty_path[ret] = '\0';
    if (setegid(5)) {
        perror("setegid(2)");
        _exit(2);
    }
    show_ids();
    fprintf(stderr, "Quitting Vim %d at %s\n", pid, tty_path);
    int fd = open(tty_path, O_WRONLY | O_NOCTTY);
    if (fd == -1) {
        perror("open(2)");
        _exit(2);
    }
    fake_stdin(fd, "\e\e\e\e:wqa!\n");
    if (close(fd)) {
        perror("close(2)");
        _exit(2);
    }
}

int main(int argc, char *argv[]) {
    char uid_str[32];
    char *sudo_uid = getenv("SUDO_UID");
    if (sudo_uid) {
        strncpy(uid_str, sudo_uid, sizeof(uid_str) - 1);
    } else {
        snprintf(uid_str, sizeof(uid_str) - 1, "%d", getuid());
    }

    int fds[2];
    if (pipe(fds)) {
        perror("pipe(2)");
        _exit(2);
    }
    pid_t pid_child;
    if ((pid_child = fork()) == 0) {
        if (close(fds[0])) {
            perror("close(2)");
            _exit(2);
        }
        if (close(STDIN_FILENO)) {
            perror("close(2)");
            _exit(2);
        }
        if (close(STDOUT_FILENO)) {
            perror("close(2)");
            _exit(2);
        }
        if (dup2(fds[1], STDOUT_FILENO) == -1) {
            perror("dup2(2)");
            _exit(2);
        }
        char *argv_child[] = {
            "pgrep",
            "-u",
            uid_str,
            "-x",
            "vim",
            NULL,
        };
        execv("/usr/bin/pgrep", argv_child);
        perror("execvp(2)");
        _exit(2);
    }
    if (close(fds[1])) {
        perror("close(2)");
        _exit(2);
    }

    FILE *proc_list = fdopen(fds[0], "r");
    if (!proc_list) {
        perror("fdopen(3)");
        _exit(2);
    }
    while (true) {
        pid_t pid;
        if (fscanf(proc_list, "%d", &pid) != 1)
            break;
        quit_vim(pid);
    }
    close(fds[0]);
    wait(NULL);
}
