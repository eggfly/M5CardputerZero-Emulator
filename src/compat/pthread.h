// Minimal pthread stub for Windows MSVC
#ifndef _PTHREAD_COMPAT_H
#define _PTHREAD_COMPAT_H

#ifdef _WIN32
#include <windows.h>
typedef CRITICAL_SECTION pthread_mutex_t;
typedef HANDLE pthread_t;
typedef void *pthread_attr_t;

#define PTHREAD_MUTEX_INITIALIZER {0}

static inline int pthread_mutex_lock(pthread_mutex_t *m) {
    // Auto-init on first use
    static int init = 0;
    if (!init) { InitializeCriticalSection(m); init = 1; }
    EnterCriticalSection(m); return 0;
}
static inline int pthread_mutex_unlock(pthread_mutex_t *m) { LeaveCriticalSection(m); return 0; }
static inline int pthread_create(pthread_t *t, const pthread_attr_t *a, void*(*fn)(void*), void *arg) {
    (void)a; *t = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)fn,arg,0,NULL); return *t?0:-1;
}
static inline int pthread_detach(pthread_t t) { CloseHandle(t); return 0; }
#endif
#endif
