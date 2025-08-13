/*
 * Copyright (C) 2025 atsb
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * =======================================================================
 *
 * Multithreaded workers
 *
 * =======================================================================
 */

#include "header/local.h"
#include <stdint.h>
#include <inttypes.h>

#ifndef GL4_MAX_THREADS
#define GL4_MAX_THREADS 8
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

static unsigned GL4_NumHWThreads(void) {
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    unsigned n = si.dwNumberOfProcessors ? si.dwNumberOfProcessors : 1u;
    if (n > GL4_MAX_THREADS) n = GL4_MAX_THREADS;
    return n;
#else
    #include <unistd.h>
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    unsigned u = (n > 0) ? (unsigned)n : 1u;
    if (u > GL4_MAX_THREADS) u = GL4_MAX_THREADS;
    return u;
#endif
}

#ifdef _WIN32
static DWORD WINAPI GL4_RowWorker(LPVOID p) {
    GL4_RowJob* j = (GL4_RowJob*)p;
    j->fn(j->row_start, j->row_end, j->user);
    return 0;
}

void GL4_ParallelTasks(int rows, int min_rows_per_task,
                                  void (*fn)(int,int,void*), void* user)
{
    unsigned threads = GL4_NumHWThreads();
    if (threads <= 1 || rows <= min_rows_per_task) {
        fn(0, rows, user);
        return;
    }

    if ((int)threads > rows) threads = (unsigned)rows;
    GL4_RowJob jobs[GL4_MAX_THREADS];
    HANDLE     th[GL4_MAX_THREADS];
    int chunk = (rows + (int)threads - 1) / (int)threads;

    int t;
    int jobcount = 0;
    for (t = 0; t < (int)threads; ++t) {
        int start = t * chunk;
        int end   = start + chunk;
        if (start >= rows) break;
        if (end > rows) end = rows;
        jobs[jobcount].row_start = start;
        jobs[jobcount].row_end   = end;
        jobs[jobcount].fn        = fn;
        jobs[jobcount].user      = user;
        th[jobcount] = CreateThread(NULL, 0, GL4_RowWorker, &jobs[jobcount], 0, NULL);
        ++jobcount;
    }
    WaitForMultipleObjects(jobcount, th, TRUE, INFINITE);
    for (t = 0; t < jobcount; ++t) CloseHandle(th[t]);
}
#else
static void* GL4_RowWorker(void* p) {
    GL4_RowJob* j = (GL4_RowJob*)p;
    j->fn(j->row_start, j->row_end, j->user);
    return NULL;
}

void GL4_ParallelTasks(int rows, int min_rows_per_task,
                                  void (*fn)(int,int,void*), void* user)
{
    unsigned threads = GL4_NumHWThreads();
    if (threads <= 1 || rows <= min_rows_per_task) {
        fn(0, rows, user);
        return;
    }

    if ((int)threads > rows) threads = (unsigned)rows;
    GL4_RowJob jobs[GL4_MAX_THREADS];
    pthread_t  th[GL4_MAX_THREADS];
    int chunk = (rows + (int)threads - 1) / (int)threads;

    int t;
    int jobcount = 0;
    for (t = 0; t < (int)threads; ++t) {
        int start = t * chunk;
        int end   = start + chunk;
        if (start >= rows) break;
        if (end > rows) end = rows;
        jobs[jobcount].row_start = start;
        jobs[jobcount].row_end   = end;
        jobs[jobcount].fn        = fn;
        jobs[jobcount].user      = user;
        pthread_create(&th[jobcount], NULL, GL4_RowWorker, &jobs[jobcount]);
        ++jobcount;
    }
    for (t = 0; t < jobcount; ++t) pthread_join(th[t], NULL);
}
#endif
