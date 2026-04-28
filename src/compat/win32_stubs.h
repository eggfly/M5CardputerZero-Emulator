#pragma once
// Stubs for Unix APIs not available on Windows/MinGW
// These make APPLaunch compile but fork/pty features won't work
#ifdef _WIN32
#include <stdlib.h>
#include <process.h>
#include <signal.h>
#include <sys/types.h>

#ifndef _WIN32_STUBS
#define _WIN32_STUBS

#ifndef WEXITSTATUS
#define WEXITSTATUS(s) ((s) & 0xff)
#define WIFEXITED(s) 1
#define WIFSIGNALED(s) 0
#define WNOHANG 1
#endif

#ifndef SIGKILL
#define SIGKILL 9
#endif

static inline pid_t fork(void) { return -1; }
static inline pid_t waitpid(pid_t p, int *s, int o) { (void)p;if(s)*s=0;(void)o; return -1; }
static inline int kill(pid_t p, int s) { (void)p;(void)s; return -1; }
static inline pid_t setsid(void) { return -1; }

struct winsize { unsigned short ws_row, ws_col, ws_xpixel, ws_ypixel; };
static inline int openpty(int *m, int *s, char *n, void *t, void *w) {
    (void)m;(void)s;(void)n;(void)t;(void)w; return -1;
}

#ifndef TIOCSCTTY
#define TIOCSCTTY 0
#endif
#ifndef F_GETFL
#define F_GETFL 3
#define F_SETFL 4
#define O_NONBLOCK 0x800
#endif
static inline int ioctl(int fd, unsigned long req, ...) { (void)fd;(void)req; return -1; }
static inline int fcntl(int fd, int cmd, ...) { (void)fd;(void)cmd; return 0; }
static inline int setenv(const char *n, const char *v, int o) { (void)o; return _putenv_s(n,v); }

#endif // _WIN32_STUBS
#endif // _WIN32
