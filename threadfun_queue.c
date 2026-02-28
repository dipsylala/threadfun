#include <windows.h>
#include <stdio.h>

#define WORKER_COUNT 4
#define JOB_COUNT 20

/*
 * Dynamic dispatch via shared queue.
 * Contrast with threadfun.c, which uses a static [start,end) partition.
 *
 * Here all threads share a single nextJob index. The CRITICAL_SECTION
 * protects the read-increment-release of that index so no job is claimed
 * by two threads and no job is skipped.
 */

typedef struct SharedQueue {
    int* jobs;
    int jobCount;
    int nextJob;
    CRITICAL_SECTION cs;
    HANDLE cancelEvent;
} SharedQueue;

DWORD WINAPI WorkerThread(LPVOID lpParam) {
    SharedQueue* q = (SharedQueue*)lpParam;

    while (1) {
        if (WaitForSingleObject(q->cancelEvent, 0) == WAIT_OBJECT_0) {
            return 0;
        }

        EnterCriticalSection(&q->cs);
        int job = -1;
        if (q->nextJob < q->jobCount) {
            job = q->jobs[q->nextJob++];
        }
        LeaveCriticalSection(&q->cs);

        if (job < 0) {
            return 0;
        }

        printf("Thread %lu processing %d\n", GetCurrentThreadId(), job);
        Sleep(200);
    }
}

int main() {
    int jobs[JOB_COUNT];
    HANDLE workers[WORKER_COUNT];
    SharedQueue q;

    for (int i = 0; i < JOB_COUNT; i++) {
        jobs[i] = i;
    }

    InitializeCriticalSection(&q.cs);
    q.cancelEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    q.jobs = jobs;
    q.jobCount = JOB_COUNT;
    q.nextJob = 0;

    for (int i = 0; i < WORKER_COUNT; i++) {
        // All threads receive the same pointer — synchronization is inside the queue.
        workers[i] = CreateThread(NULL, 0, WorkerThread, &q, 0, NULL);
    }

    // Early-stop path: SetEvent(q.cancelEvent);
    WaitForMultipleObjects(WORKER_COUNT, workers, TRUE, INFINITE);

    for (int i = 0; i < WORKER_COUNT; i++) {
        CloseHandle(workers[i]);
    }
    CloseHandle(q.cancelEvent);
    DeleteCriticalSection(&q.cs);

    return 0;
}
