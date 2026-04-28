#pragma once
#ifdef _WIN32
#include <stdlib.h>
#include <process.h>
#ifndef _SYS_WAIT_STUBS
#define _SYS_WAIT_STUBS
typedef int pid_t;
#define WEXITSTATUS(s) ((s) & 0xff)
#define WIFEXITED(s) 1
#define WIFSIGNALED(s) 0
#define WNOHANG 1
static inline pid_t fork(void) { return -1; }
static inline pid_t waitpid(pid_t p, int *s, int o) { (void)p;if(s)*s=0;(void)o; return -1; }
static inline int kill(pid_t p, int s) { (void)p;(void)s; return -1; }
#ifndef SIGTERM
#define SIGTERM 15
#endif
#ifndef SIGKILL
#define SIGKILL 9
#endif
// openpty stub
static inline int openpty(int *m, int *s, char *n, void *t, void *w) {
    (void)m;(void)s;(void)n;(void)t;(void)w; return -1;
}
// execlp stub
static inline int execlp(const char *f, ...) { (void)f; return -1; }
#endif
#endif
