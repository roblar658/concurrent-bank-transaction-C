#ifndef SYNC_PRIMITIVES_H
#define SYNC_PRIMITIVES_H

#if defined(_WIN32) || defined(_WIN64)
#define PLATFORM_WINDOWS 1
#include <windows.h>
#include <process.h>

typedef CRITICAL_SECTION bank_mutex_t;

static inline void bank_mutex_init(bank_mutex_t* m) {
    InitializeCriticalSection(m);
}

static inline void bank_mutex_lock(bank_mutex_t* m) {
    EnterCriticalSection(m);
}

static inline int bank_mutex_trylock(bank_mutex_t* m) {
    return TryEnterCriticalSection(m) ? 1 : 0;
}

static inline void bank_mutex_unlock(bank_mutex_t* m) {
    LeaveCriticalSection(m);
}

static inline void bank_mutex_destroy(bank_mutex_t* m) {
    DeleteCriticalSection(m);
}

// Windows native tråd-abstraksjon
typedef HANDLE bank_thread_t;
typedef unsigned (__stdcall *bank_thread_func_t)(void*);

static inline int bank_thread_create(bank_thread_t* t, bank_thread_func_t func, void* arg) {
    *t = (HANDLE)_beginthreadex(NULL, 0, func, arg, 0, NULL);
    return (*t != NULL) ? 0 : -1;
}

static inline void bank_thread_join(bank_thread_t t) {
    WaitForSingleObject(t, INFINITE);
    CloseHandle(t);
}

static inline void bank_sleep_ms(unsigned int ms) {
    Sleep(ms);
}

#else
// POSIX Fallback (Linux / macOS)
#include <pthread.h>
#include <unistd.h>

typedef pthread_mutex_t bank_mutex_t;

static inline void bank_mutex_init(bank_mutex_t* m) {
    pthread_mutex_init(m, NULL);
}

static inline void bank_mutex_lock(bank_mutex_t* m) {
    pthread_mutex_lock(m);
}

static inline int bank_mutex_trylock(bank_mutex_t* m) {
    return (pthread_mutex_trylock(m) == 0) ? 1 : 0;
}

static inline void bank_mutex_unlock(bank_mutex_t* m) {
    pthread_mutex_unlock(m);
}

static inline void bank_mutex_destroy(bank_mutex_t* m) {
    pthread_mutex_destroy(m);
}

typedef pthread_t bank_thread_t;
typedef void* (*bank_thread_func_t)(void*);

static inline int bank_thread_create(bank_thread_t* t, bank_thread_func_t func, void* arg) {
    return pthread_create(t, NULL, func, arg);
}

static inline void bank_thread_join(bank_thread_t t) {
    pthread_join(t, NULL);
}

static inline void bank_sleep_ms(unsigned int ms) {
    usleep(ms * 1000);
}
#endif

// Rettferdig FIFO Ticket Lock (Starvation-free) ved bruk av C11 atomics
#include <stdatomic.h>

typedef struct {
    atomic_uint ticket;
    atomic_uint turn;
} ticket_mutex_t;

static inline void ticket_mutex_init(ticket_mutex_t* tm) {
    atomic_init(&tm->ticket, 0);
    atomic_init(&tm->turn, 0);
}

static inline void ticket_mutex_lock(ticket_mutex_t* tm) {
    unsigned int my_turn = atomic_fetch_add_explicit(&tm->ticket, 1, memory_order_relaxed);
    while (atomic_load_explicit(&tm->turn, memory_order_acquire) != my_turn) {
#if defined(PLATFORM_WINDOWS)
        YieldProcessor();
#else
        #if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        __builtin_ia32_pause();
        #else
        sched_yield();
        #endif
#endif
    }
}

static inline void ticket_mutex_unlock(ticket_mutex_t* tm) {
    unsigned int current = atomic_load_explicit(&tm->turn, memory_order_relaxed);
    atomic_store_explicit(&tm->turn, current + 1, memory_order_release);
}

#endif // SYNC_PRIMITIVES_H
