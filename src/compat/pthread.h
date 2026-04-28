// Minimal pthread stub for Windows and Emscripten (single-thread mode)
#ifndef _PTHREAD_COMPAT_H
#define _PTHREAD_COMPAT_H

#if defined(_WIN32)
#include <windows.h>
typedef CRITICAL_SECTION pthread_mutex_t;
typedef HANDLE pthread_t;
typedef void *pthread_attr_t;
#define PTHREAD_MUTEX_INITIALIZER {0}
static inline int pthread_mutex_lock(pthread_mutex_t *m) {
    static int init = 0;
    if (!init) { InitializeCriticalSection(m); init = 1; }
    EnterCriticalSection(m); return 0;
}
static inline int pthread_mutex_unlock(pthread_mutex_t *m) { LeaveCriticalSection(m); return 0; }
static inline int pthread_create(pthread_t *t, const pthread_attr_t *a, void*(*fn)(void*), void *arg) {
    (void)a; *t = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)fn,arg,0,NULL); return *t?0:-1;
}
static inline int pthread_detach(pthread_t t) { CloseHandle(t); return 0; }

#elif defined(__EMSCRIPTEN__)
#include <stdint.h>
typedef int pthread_mutex_t;
typedef int pthread_t;
typedef void *pthread_attr_t;
#define PTHREAD_MUTEX_INITIALIZER 0
static inline int pthread_mutex_lock(pthread_mutex_t *m) { (void)m; return 0; }
static inline int pthread_mutex_unlock(pthread_mutex_t *m) { (void)m; return 0; }
static inline int pthread_create(pthread_t *t, const pthread_attr_t *a, void*(*fn)(void*), void *arg) {
    (void)t; (void)a; (void)fn; (void)arg; return -1;
}
static inline int pthread_detach(pthread_t t) { (void)t; return 0; }
#endif

#endif
