#include <windows.h>
#include <stdio.h>

#define WORKER_COUNT 4
#define JOB_COUNT 20

typedef struct ThreadContext {
    int* jobs;
    int start;
    int end;
    HANDLE cancelEvent;
} ThreadContext;

/*
 * No mutex/semaphore is required for job access in this design:
 * - `jobs` is initialized once, then treated as read-only by all threads.
 * - each thread gets a disjoint [start, end) range (no overlapping writes/reads).
 * - main waits for all workers before leaving scope, so job/context memory stays valid.
 */

DWORD WINAPI WorkerThread(LPVOID lpParam) {
    ThreadContext* ctx = (ThreadContext*)lpParam;

    for (int i = ctx->start; i < ctx->end; i++) {
        if (WaitForSingleObject(ctx->cancelEvent, 0) == WAIT_OBJECT_0) {
            return 0;
        }
        printf("Thread %lu processing %d\n",
               GetCurrentThreadId(), ctx->jobs[i]);
        Sleep(200);
    }

    return 0;
}

int main() {
    HANDLE cancelEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    int jobs[JOB_COUNT];
    HANDLE workers[WORKER_COUNT];
    ThreadContext contexts[WORKER_COUNT];

    for (int i = 0; i < JOB_COUNT; i++) {
        jobs[i] = i;
    }

    for (int i = 0; i < WORKER_COUNT; i++) {
        // Static partition: each worker gets a fixed slice [start, end).
        contexts[i].jobs = jobs;
        contexts[i].start = (i * JOB_COUNT) / WORKER_COUNT;
        contexts[i].end = ((i + 1) * JOB_COUNT) / WORKER_COUNT;
        contexts[i].cancelEvent = cancelEvent;

        workers[i] = CreateThread(
            NULL,
            0,
            WorkerThread,
            &contexts[i],
            0,
            NULL
        );
    }

    // Early-stop path: SetEvent(cancelEvent);
    WaitForMultipleObjects(WORKER_COUNT, workers, TRUE, INFINITE);

    for (int i = 0; i < WORKER_COUNT; i++) {
        CloseHandle(workers[i]);
    }
    CloseHandle(cancelEvent);

    return 0;
}
