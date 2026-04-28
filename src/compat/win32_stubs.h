#pragma once
#ifdef _WIN32
#include <stdlib.h>
#include <process.h>
#include <signal.h>

#ifndef _SYS_WAIT_STUBS
#define _SYS_WAIT_STUBS

#ifndef WEXITSTATUS
#define WEXITSTATUS(s) ((s) & 0xff)
#endif
#ifndef WIFEXITED
#define WIFEXITED(s) 1
#endif
#ifndef WIFSIGNALED
#define WIFSIGNALED(s) 0
#endif
#ifndef WNOHANG
#define WNOHANG 1
#endif

// fork/waitpid/kill — MinGW doesn't have these
#ifndef __FORK_STUB_DEFINED
#define __FORK_STUB_DEFINED
#include <sys/types.h>  // pid_t on MinGW
static inline pid_t fork(void) { return -1; }
static inline pid_t waitpid(pid_t p, int *s, int o) { (void)p;if(s)*s=0;(void)o; return -1; }
#endif

// kill stub
#ifndef kill
static inline int kill(pid_t p, int s) { (void)p;(void)s; return -1; }
#endif
#ifndef SIGKILL
#define SIGKILL 9
#endif

// PTY stubs (openpty, setsid, winsize)
struct winsize { unsigned short ws_row, ws_col, ws_xpixel, ws_ypixel; };
static inline int openpty(int *m, int *s, char *n, void *t, void *w) {
    (void)m;(void)s;(void)n;(void)t;(void)w; return -1;
}
static inline pid_t setsid(void) { return -1; }
static inline int dup2(int o, int n) { (void)o;(void)n; return -1; }

#endif // _SYS_WAIT_STUBS
#endif // _WIN32
