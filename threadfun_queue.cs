// Dynamic dispatch via shared queue.
// Contrast with threadfun.cs, which uses a static [start,end) partition.
//
// Here all tasks share a single SharedQueue. The lock protects
// the read-increment-release of _nextJob so no job is claimed by two tasks
// and no job is skipped.

using System;
using System.Threading;
using System.Threading.Tasks;

internal static class Program
{
    private const int WorkerCount = 4;
    private const int JobCount = 20;

    private sealed class SharedQueue
    {
        private readonly int[] _jobs;
        private int _nextJob;
        private readonly object _lock = new object();

        public SharedQueue(int[] jobs)
        {
            _jobs = jobs;
        }

        public bool TryPop(out int job)
        {
            lock (_lock)
            {
                if (_nextJob >= _jobs.Length)
                {
                    job = -1;
                    return false;
                }
                job = _jobs[_nextJob++];
                return true;
            }
        }
    }

    private static void Worker(int id, SharedQueue queue, CancellationToken cancelToken)
    {
        while (!cancelToken.IsCancellationRequested)
        {
            if (!queue.TryPop(out int job))
            {
                return;
            }
            Console.WriteLine($"Thread {Environment.CurrentManagedThreadId} processing {job}");
            Thread.Sleep(200);
        }
    }

    private static void Main()
    {
        using var cts = new CancellationTokenSource();
        var jobs = new int[JobCount];
        for (int i = 0; i < JobCount; i++)
        {
            jobs[i] = i;
        }

        // All tasks receive the same reference — synchronization is inside the queue.
        var queue = new SharedQueue(jobs);
        var workers = new Task[WorkerCount];

        for (int i = 0; i < WorkerCount; i++)
        {
            int id = i;
            workers[i] = Task.Run(() => Worker(id, queue, cts.Token));
        }

        // Early-stop path: cts.Cancel();
        // Join all workers (WaitForMultipleObjects(..., TRUE, ...) equivalent).
        Task.WaitAll(workers);
    }
}
